/**
 * OAuth2 in C/C++
 * Go straight for flow that uses Serverless version
 * to get information about the login
 */

#include "url.h"
#include "tiny_web_client.h"
#include "random_string.h"
#include "json_parser.h"
#include "open_browser.h"
#include "tiny_web_server.h"


int main()
{
    std::cout << "==============================================\n"
              << "(Public API call) GetApplicationEndpoint\n"
              << "==============================================" << std::endl;
    const URL target(static_cast<const std::ostringstream&>(
            std::ostringstream() << "https://" << API_HOST
                                 << API_APPLICATION_ENDPOINT_PATH).str());
    Request request = make_request(target);
    Response response;
    const auto resp = http_send(request, response);
    if (  resp != 0 ) {
        std::cerr << "resp: " << resp << std::endl;
        throw std::runtime_error("request failed");
    }
    std::cout << "Body: " << response.body << '\n';

    const std::string temporary_secret_state = generate_random_string(5);
    std::cout << "Generated secret state: " << temporary_secret_state << std::endl;

    const JsonItem metadata = json_create_from_string(response.body);
#ifdef TEST_JSON
    json_pretty_print(metadata);
#endif

    const auto openid_json = metadata.object.find("openid");
    std::cout << "OpenID: " << openid_json->second.text << "\n"

              << "==============================================\n"
              << "(Public API call) OpenID Metadata Call\n"
              << "==============================================" << std::endl;
    Request openid_request = make_request(URL(openid_json->second.text));
    Response openid_response;
    if ( http_send(openid_request, openid_response) != 0 ) {
        throw std::runtime_error("request failed");
    }

    const JsonItem openid_metadata = json_create_from_string(openid_response.body);
#ifdef TEST_JSON
    json_pretty_print(openid_metadata);
#endif

    const auto authorization_endpoint_json = openid_metadata.object.find("authorization_endpoint");
    const std::string authorization_endpoint = authorization_endpoint_json->second.text;

    std::cout << "==============================================\n"
              << "Send user to browser\n"
              << "==============================================" << std::endl;
    const std::string redirect_uri = static_cast<const std::ostringstream&>(
            std::ostringstream() << "http://" << SERVER_HOST << ':'
                                 << PORT_TO_BIND << EXPECTED_PATH).str();
    URL authorization_url = URL(authorization_endpoint);
    authorization_url.add_param("response_type", "code");
    authorization_url.add_param("client_id", metadata.object.find("clientId")->second.text);
    authorization_url.add_param("redirect_uri", redirect_uri);
    authorization_url.add_param("state", temporary_secret_state);
    authorization_url.add_param("scope", "openid");
    open_browser(authorization_url);

    // we then need to start our web server and block
    // until we get the appropriate response
    // TODO make details no longer macros
    std::cout << "==============================================\n"
              << "Await response to be passed from browser to local web server\n"
              << "==============================================" << std::endl;
    const AuthenticationResponse oauth_response = wait_for_oauth2_redirect();

    
    // GET /ibm/cloud/appid/callback?code=fVTDrC7Dt8KyG2jCv8OQEMOCTS4TWsK9AULCmx7Dl8KXw5wKJAjDv1rDiMKCw6TCncOkIC95KFfCpDMrGmzCusOCwrB4ZcKTZSFCwrc1wrPClwVtwpzDg3knw57DoMKvwoLDrsOfWcONB8OHwprDvcKzesKTVsOSdCrCindIw4RFOTnCgjzDrwvCq15EwrZJUMK5PsOaNks8F8KVw7fCi3pCwpQEdSYGLGNAwrzDoMO5KRw2w7HDkiIIA8OtNyTDrcKOwrvCv8Otw5jDksODBcKeB3XDtcKCfcK2wr7ChHXChkDCkGDDi2XDjsO7w6DDj8KDw5hFw5jDqGoMw7LDmhnCiMKUwo4Cw5QSw4HDoy5hFklqwpJ4wo_CsRxXNw&state=3otgw HTTP/1.1
    // Host: localhost:3000
    // User-Agent: Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:83.0) Gecko/20100101 Firefox/83.0
    // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
    // Accept-Language: en-GB,en;q=0.5
    // Accept-Encoding: gzip, deflate
    // Connection: keep-alive
    // Cookie: .AspNetCore.Cookies=CfDJ8PB0mYqBkIZEkM1UBJ7BKURUGzbrmg4MKt5wi0WeBMuIpuUXjfgi-vrBRPaTOi4_d4h6s7z8_x7Kv78YpMdXABnUh2OGRNN456SoqIPkOknw9KCwvthbVXb3fQfBSTD47BRcplqTFmZSZ9T4EgcqEm8Y2oCaVsZTjRlCcijipQkPP0JsOCc241xiVWZXwqRtrj_pSFSl1DDjyTDmaQ75iUwEibnwXI6vOtSeb8G010ZVXQZs5xbugFJuNjm1SCILqrwMigVkuAmNxCzkULyFkpriLBlS6q9fVx50YAu4vPRZGtJE83nTiv5R_Y1DV1g2Qvo1ZeUpUzAo8Kt5PwVKR-A2rXQKnOJoccY4zrbe4lRG2qZ-RlZ4iBzf167TS1mAjxuAOrIvyofVr__fG-XkHnCCLD9r5gnr6BPnm374SoOGr3e_1iTxoc7Bk0a0-xTrLPaGZhzyL-s1Nz8ZKHk3BqXmeecoLwbDt2gTjxeS-hnsdrC4qT7_2bljAnMtU3BQ9iIoXQi9Yv-P_GhzaqlsBvq_TRcjiD2DJHT6yV7GI8yJetVwY7NNXFrmPrznkMl90QuA6wMc0C-XbHYNFc-6EsoImC1_f2aq7VgkBEp2wf8Ij0ZWSvgqdJ88T3WiK4GtXB55v_JDM-6c6I0smY9cYi0-HArlHPljUOS6_KacBetsbl49bKhU6NxZKnKgNRQVlrTXMmrBwt-h2f8b-pXuj0bkZgO-lpBUTI2FEaJnRfAOFykmylT-Ov4dLqYAkFGBFtbMKs6ppxDu0462-cKg0unuwBxriw8tXYNXzXrh6b33wdpns-Rwgeh7X_R9f3sPFSDCUk0qxv9JDr3C6HHcglsTQdcXCmmpxeSinjY6Fi9k2aIc-xIiZDkyjMDo10HirUiY9MuNKVkrj9C66pliYL5u-xVmvF1f51heRkGf1LSFxlhzijAyX_WB9siwGRU54aUBdMA_ww7cn0zks-PnsjH-RTRKBek98oOm_uJ5G2yPW-EAuKZdMl8V0xYJBF3b_iPWqCQ06rkTKpYN8rHR2pADH2idcUIkT374UuZ_ptY0-tO7UoiNfDs89eYLhEjpeuotnM6sCwLdTO9kF6QEaVuXtzg4ivAdGu42msRdu40SjhTsoCd1DCPPmqPNHf23RHPHUB1fOfdoSKLRmT9UHuSDAxqKFHWb1o1iQABtrpUS6N02tHGn92_-zT_tF0xb8l9QAQDj2YSzAnx0UjpceEIAVGhRRMNulWO-Tzgu8GcLTVQ1T2PpJYtRJsSabVjI6plGF3bKlZUeWG9DHdtvCSdwqYZoWHQjw7YaMFFPcYMTa9GqmkXsAEg6_aVID56WY4Pnkj7TU6N4tpdTl6MOL-P1lHUKtWJHsda44MtVGc5x4C8C8k7whEOJ0vWtDEBTlHODluP6ce16IZOG8Gi3dIcRg1MF7t3pKg6I0PN3ygFVMf7u05KeNq5A8NO9xtzfsLzEiaK0DMgmhnz4msDu3VtoZHl-fU-gmVNPwLMs962dUvHUms9i_sDdXwjuASNZvzorLzrfRNmhHYAVaTYCZdalAtXMkHZv3Hh3WJ-VXn5Y4eevX_VC88cNGSM9s5zYajZTUZ0LTaA7seYVFoxFKk80Vd5d_7sal7jJI7hTQAqSlwumr_GzBhsq9OD6NjlLrwYw0DthGO0V-CskAhJJ6rvQ0KhsVgbOzEPxDsdp_8iQhqxbuaQejxE_5WMmNUWDIUsJ0BHUDMdxpXZdIdH2n6s; connect.sid=s%3Anl3r6kNI5uoXAAnd2fF340b18GEMuP00.n%2Fi%2F41RWdj%2B76wNAVr%2B41slPZrEQEhKg3NXJXicgbxo
    // Upgrade-Insecure-Requests: 1

    // we still need to parse out the code and state secret.
    std::cout << oauth_response.raw << '\n'
              << oauth_response.code << '\n'
              << oauth_response.secret << std::endl;

    if ( oauth_response.secret != temporary_secret_state ) {
        throw std::runtime_error(static_cast<const std::ostringstream&>(
                                         std::ostringstream() <<"oauth2 redirect contained the wrong secret state ("
                                                              << oauth_response.secret << ","
                                                              << "expected " << temporary_secret_state << '\n').str());
    } else {
        std::cout << "Secret state was successfully retrieved from redirect url.\n";
    }

    // get the access token
    std::cout << "==============================================\n"
              << "(Public API+secret) GetAccessToken\n"
              << "==============================================" << std::endl;
    const URL token_url = URL(
            static_cast<const std::ostringstream&>(
                    std::ostringstream() << "https://" << API_HOST << API_GET_ACCESS_TOKEN_PATH).str());
    const std::map<std::string, std::string> post_fields {
            std::make_pair("grant_type", "authorization_code"),
            std::make_pair("code", oauth_response.code),
            std::make_pair("redirect_uri", redirect_uri),
            std::make_pair("client_id", metadata.object.find("clientId")->second.text)
    };
    Request token_request = make_request(token_url, "POST");
    Response token_response;
    if ( http_send(token_request, token_response, post_fields) != 0 ) {
        throw std::runtime_error("request failed to get token");
    }
    std::cout << token_response.raw << std::endl;

    const JsonItem access_json = json_create_from_string(token_response.body);
    const std::string access_token = access_json.object.find("access_token")->second.text;
    std::cout << "Access Token: " << access_token << '\n';

    // get user details to prove we are looked and show
    // how to pass bearer token
    std::cout << "==============================================\n"
              << "(Published Private API) UserInfo\n"
              << "==============================================" << std::endl;
    const auto userinfo_endpoint_json = openid_metadata.object.find("userinfo_endpoint");
    const std::string userinfo_endpoint = userinfo_endpoint_json->second.text;
    const URL userinfo_url = URL(userinfo_endpoint);
    Request userinfo_request = make_request(userinfo_url);
    userinfo_request.headers.emplace_back("Content-type: application/json");
    userinfo_request.headers.push_back("Authorization: Bearer " + access_token);
    Response userinfo_response;
    if ( http_send(userinfo_request, userinfo_response) ) {
        throw std::runtime_error("request failed to get userinfo");
    }
    std::cout << userinfo_response.raw << '\n';

    // Get contents of a private url
    std::cout << "==============================================\n"
              << "(Our Private API) Hello\n"
              << "==============================================" << std::endl;
    Request private_request = make_request(URL("https://31f5ff35.eu-gb.apigw.appdomain.cloud/private-authtest/Hello"));
    private_request.headers.emplace_back("Content-type: application/json");
    private_request.headers.push_back("Authorization: Bearer " + access_token);
    Response private_response;
    if ( http_send(private_request, private_response) ) {
        throw std::runtime_error("request failed to get userinfo");
    }
    std::cout << private_response.raw << '\n';
}
