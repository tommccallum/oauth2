// Reference https://github.com/microsoft/cpprestsdk/blob/7fbb08c491f9c8888cc0f3d86962acb3af672772/Release/samples/Oauth1Client/Oauth1Client.cpp
#include <iostream>
#include <cstdlib>
#include <sstream>

#ifdef __linux__
#include <cstring>
#elif defined(macintosh) || defined(Macintosh) || defined(__APPLE__) && defined(__MACH__)
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef TEST_OPEN_BROWSER
#include "url.cpp"
#else
// here to make this not have red squiggles
// we forward declare the URL class and its
// operator<< function.
class URL;
std::ostream& operator<<(std::ostream& out, URL& url);
#endif

// We could have used a plain string instead of
// creating a URL type, or we could use a url type
// from a library, but here we create our own
// for explanations sake.
static
void open_browser(URL& url)
{
#ifdef __linux__
    // On linux xdg-open is a command that opens the
    // preferred application for the type of file or url.
    // For more information use: man xdg-open on the
    // terminal command line.
    //
    // In OAuth2 we open the browser for the user to
    // enter their credentials.
    std::ostringstream browser_cmd;
    browser_cmd << "xdg-open \"";
    browser_cmd << url;
    browser_cmd << '"';
    auto browser_cmd_string = browser_cmd.str();
    std::cout << browser_cmd_string << std::endl;
    (void)system(browser_cmd_string.c_str());

#elif defined(macintosh) || defined(Macintosh) || defined(__APPLE__) && defined(__MACH__)

    std::ostringstream _url;
    _url << url;
    auto url_str = _url.str();
    CFURLRef cf_url = CFURLCreateWithBytes (
            NULL,                        // allocator
            (UInt8*)url_str.c_str(),     // URLBytes
            (signed long)url_str.length(),            // length
            kCFStringEncodingASCII,      // encoding
            NULL                         // baseURL
    );
    LSOpenCFURLRef(cf_url,0);
    CFRelease(cf_url);
#endif
}

#ifdef TEST_OPEN_BROWSER
int main() {
    URL url("http://www.google.com");
    open_browser(url);
}
#endif
