// SSL Example Reference: https://stackoverflow.com/questions/41229601/openssl-in-c-socket-connection-https-client
// Client Reference: https://stackoverflow.com/questions/22077802/simple-c-example-of-doing-an-http-post-and-consuming-the-response
#include <algorithm>
#include <string>
#include <map>
#include <map>
#include <sstream>
#include <iostream>
#include <cstdio>       /* printf, sprintf */
#include <cstdlib>      /* exit */
#include <cstring>      /* memcpy, memset */
#include "config.h"
#include "tiny_web_client.h"

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <io.h>
#include <winsock2.h>

// SSL
#else
#include <unistd.h>     /* read, write, close */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#endif

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#ifdef TEST_TINY_WEB_CLIENT
#include "json_parser.cpp"
#endif

void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

std::string create_host(Request const &request)
{
    std::ostringstream ss;
    ss << request.uri.host;
    if (request.uri.port != 0 && request.uri.port != 80)
    {
        ss << ":" << request.uri.port;
    }
    return ss.str();
}

std::string create_message(Request const &request)
{
    //char *message_fmt = "GET / HTTP/1.0\r\n\r\n";
    std::ostringstream oss;
    oss << request.verb
        << " "
        << request.uri.path;
    if ( !request.uri.querystring.empty() ) {
        oss << "?" << request.uri.querystring;
    }
    oss << " "
    // @TODO add in support for querystring/parameters
        << request.uri.protocol
        << "/"
        << request.uri.protocol_version
        << "\r\n";
    for( const std::string& header : request.headers )
        oss << header << "\r\n";

    oss << "\r\n";
    return oss.str();
}

Request make_request(const URL &u, const std::string &verb) {
    Request req;
    req.verb = verb;
    req.uri.protocol = "HTTP";
    req.uri.protocol_version = "1.0";
    if ( u.protocol ==  "https") {
        req.uri.use_ssl = true;
        req.uri.port = 443;
    } else {
        req.uri.use_ssl = false;
        req.uri.port = 80;
    }
    req.uri.host = u.domain;
    req.uri.path = u.path;
    req.uri.querystring = u.encoded_querystring();
    req.uri.fragment = u.fragment;
    req.headers.push_back("HOST: "+req.uri.host);
    return req;
}

// Using the RAII idiom to ensure our SSL resource is cleaned up
[[nodiscard]] bool SSLClient::is_valid() const {
    return valid_;
}

bool SSLClient::connect_to_socket(int socket_file_descriptor)
{
    if (!init_class())
    {
        return false;
    }
    ssl_socket_file_descriptor_ = SSL_get_fd(session_);
    SSL_set_fd(session_, socket_file_descriptor);
    int err = SSL_connect(session_);
    if (err <= 0)
    {
        printf("Error creating SSL connection.  err=%x\n", err);
        fflush(stdout);
        display_errors();
        shutdown();
        return false;
    }
    printf("SSL connection using %s\n", SSL_get_cipher(session_));
    return true;
}

SSLClient::~SSLClient()
{
    shutdown();
}

SSL *SSLClient::session() {
    return session_;
}

int http_send(Request&request, Response &response, const std::map<std::string, std::string> &post_fields)
{
    std::string content;
    if ( request.verb == "POST" ) {
        if ( post_fields.empty() ) {
            throw std::runtime_error("request was POST, but no post fields given to http_send");
        }
        std::ostringstream ss;
        {
            size_t counter = 0;
            for (auto const &[key, value] : post_fields) {
                // TODO encode value for special characters
                ss << key << "=" << value;
                if (counter < post_fields.size() - 1) {
                    ss << "&";
                }
                counter++;
            }
        }
        content = ss.str();
        request.headers.emplace_back("Content-Type: application/x-www-form-urlencoded");
        std::cout << "POST Data: " << content << '\n';

        request.headers.push_back(static_cast<const std::ostringstream&>(
                std::ostringstream() << "Content-Length: " << content.size()).str());
    }
    std::string message = create_message(request);
    if ( !content.empty() ) {
        message += content + "\r\n";
    }
    std::cout << "Target: " << create_host(request) << '\n'
              << "Sending: " << message;

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup returned: " << iResult << std::endl;
        return iResult;
    }
#endif

    struct hostent *server;
    struct sockaddr_in serv_addr{};
    SSLClient ssl_client;

    /* create the socket */
    int socket_file_descriptor = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (socket_file_descriptor < 0)
    {
        response.error.message = "ERROR opening socket";
        response.error.code = 1001;
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
        std::cerr << "WSAGetLastError: " << WSAGetLastError() << std::endl;
#endif
        return response.error.code;
    }

    /* lookup the ip address */
    server = gethostbyname(request.uri.host.c_str());
    if (server == NULL)
    {
        response.error.message = "ERROR no such host";
        response.error.code = 1002;
        return response.error.code;
    }

    /* fill in the structure */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    ZeroMemory(&serv_addr, sizeof(serv_addr));
#else
    memset(&serv_addr, 0, sizeof(serv_addr));
#endif
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(request.uri.port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    /* connect the socket */
    if (connect(socket_file_descriptor, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        response.error.message = "ERROR connecting";
        response.error.code = 1003;
        return response.error.code;
    }

    if (request.uri.use_ssl)
    {
        ssl_client.connect_to_socket(socket_file_descriptor);
        if (!ssl_client.is_valid())
        {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
            closesocket(socket_file_descriptor);
#else
            close(socket_file_descriptor);
#endif
            response.error.message = "ERROR failed to open ssl connection";
            response.error.code = 1100;
            return response.error.code;
        }
    }

    /* send the request */

    int bytes, total = (int)message.size(), sent = 0;
    do
    {
        if (ssl_client.is_valid())
        {
            bytes = SSL_write(ssl_client.session(), message.c_str(), (int)message.size());
            response.error.code = 1004;
            if (bytes < 0)
            {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
                closesocket(socket_file_descriptor);
#else
                close(socket_file_descriptor);
#endif
                int err = SSL_get_error(ssl_client.session(), bytes);
                switch (err)
                {
                case SSL_ERROR_WANT_WRITE:
                    response.error.message = "ERROR writing to ssl socket SSL_ERROR_WANT_WRITE";
                    break;
                case SSL_ERROR_WANT_READ:
                    response.error.message = "ERROR writing to ssl socket SSL_ERROR_WANT_READ";
                    break;
                case SSL_ERROR_ZERO_RETURN:
                    response.error.message = "ERROR writing to ssl socket SSL_ERROR_ZERO_RETURN";
                    break;
                case SSL_ERROR_SYSCALL:
                    response.error.message = "ERROR writing to ssl socket SSL_ERROR_SYSCALL";
                    break;
                case SSL_ERROR_SSL:
                    response.error.message = "ERROR writing to ssl socket SSL_ERROR_SSL";
                    break;
                default:
                    response.error.message = "ERROR unknown ssl error when writing to socket";
                    break;
                }
                return response.error.code;
            }
        }
        else
        {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
            bytes = send(socket_file_descriptor, message.c_str() + sent, total - sent, 0);
#else
            bytes = write(socket_file_descriptor, message.c_str() + sent, total - sent);
#endif
            if (bytes < 0)
            {
                response.error.message = "ERROR writing message to socket";
                response.error.code = 1004;
                return response.error.code;
            }
            if (bytes == 0)
                break;
        }
        sent += bytes;
    } while (sent < total);

    /* receive the response */
    std::ostringstream incoming_data;
    char buffer[STACK_SIZE];
    // SECURITY make sure we wipe the buffer as in previous runs
    // we had some leakage into the next call.
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    ZeroMemory(&buffer, sizeof(buffer));
#else
    memset(&buffer, 0, sizeof(buffer));
#endif
    total = sizeof(buffer) - 1;
    int received = 0;
    do
    {
        if (ssl_client.is_valid())
        {
            bytes = SSL_read(ssl_client.session(), buffer + received, total - received);
        }
        else
        {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
            bytes = recv(socket_file_descriptor, buffer + received, total - received, 0);
#else
            bytes = read(socket_file_descriptor, buffer + received, total - received);
#endif
        }
        if (bytes < 0)
        {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
            closesocket(socket_file_descriptor);
#else
            close(socket_file_descriptor);
#endif
            // check for ssl errors if we are using ssl
            if (ssl_client.is_valid())
            {
                // https://www.openssl.org/docs/man1.1.1/man3/SSL_get_error.html
                int err = SSL_get_error(ssl_client.session(), bytes);
                switch (err)
                {
                case SSL_ERROR_WANT_READ:
                    response.error.message = "ERROR SSL_ERROR_WANT_READ";
                    break;
                case SSL_ERROR_WANT_WRITE:
                    response.error.message = "ERROR SSL_ERROR_WANT_WRITE";
                    break;
                case SSL_ERROR_ZERO_RETURN:
                    response.error.message = "ERROR SSL_ERROR_ZERO_RETURN";
                    break;
                case SSL_ERROR_SYSCALL:
                    response.error.message = "ERROR SSL_ERROR_SYSCALL";
                    break;
                case SSL_ERROR_SSL:
                    response.error.message = "ERROR SSL_ERROR_SSL";
                    break;
                default:
                    response.error.message = "ERROR Unknown ssl error";
                    break;
                }
            }
            else
            {
                response.error.message = "ERROR reading response from socket";
            }
            response.error.code = 1005;
            return response.error.code;
        }
        if (bytes == 0)
            break;
        received += bytes;
        if (received == total)
        {
            incoming_data << buffer;
            // reset to nulls
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
            ZeroMemory(&buffer, sizeof(buffer));
#else
            memset(&buffer, 0, sizeof(buffer));
#endif
            received = 0;
        }
    } while (true);
    incoming_data << buffer;

    /* close the socket */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    closesocket(socket_file_descriptor);
    WSACleanup();
#else
    close(socket_file_descriptor);
#endif
    response.raw = incoming_data.str();
    for (size_t ii = 0, pos=0; ii < response.raw.size(); ii++)
    {
        if (response.raw[ii] == '\r' && response.raw[ii + 1] == '\n' &&
            response.raw[ii + 2] == '\r' && response.raw[ii + 3] == '\n')
        {
            response.body = response.raw.substr(ii + 4, response.raw.size() - (ii + 4));
            break;
        }
        if (response.raw[ii] == '\r' && response.raw[ii + 1] == '\n')
        {
            std::string header = response.raw.substr(pos, ii - pos);
            std::cout << "HEADER: " << header << std::endl;
            response.headers.push_back(header);
            if (response.headers.size() == 1)
            {
                size_t start = header.find_first_of(' ') + 1,
                       end = header.find_last_of(' ');
                std::string status = header.substr(start, end - start);
                response.status = std::stol(status);
            }
            std::string lc_header = header;
            std::transform(lc_header.begin(), lc_header.end(), lc_header.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lc_header.find("content-type: ") == 0)
            {
                const size_t start = lc_header.find(' ') + 1;
                /* std::remove_const<decltype(std::string::npos)>::type */
                size_t end = lc_header.find(';');
                if (end == std::string::npos)
                {
                    end = lc_header.size();
                }
                response.content_type = lc_header.substr(start, end - start);
            }
            pos = ii + 2;
        }
    }

    return 0;
}

#ifdef TEST_TINY_WEB_CLIENT
int main()
{
    Request req = Request{};
    req.verb = "GET";
    req.uri.use_ssl = true;
    req.uri.protocol = "HTTP";
    req.uri.protocol_version = "1.0";
    // https://31f5ff35.eu-gb.apigw.appdomain.cloud/authtest/GetApplicationEndpoint
    req.uri.host = STR(API_HOST);
    req.uri.port = 443;
    req.uri.path = STR(API_APPLICATION_ENDPOINT_PATH);
    req.headers.push_back("HOST: " + STR(API_HOST));
    Response resp = Response{};

    if (http_send(req, resp))
    {
        std::cout << resp.error.message << std::endl;
    }
    else
    {
        std::cout << resp.status << "\n"
                  << resp.content_type << "\n"
                  << resp.body << std::endl;
    }
}
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#undef _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
