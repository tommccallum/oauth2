#ifndef OAUTH2_OPEN_BROWSER_H
#define OAUTH2_OPEN_BROWSER_H

// Reference https://github.com/microsoft/cpprestsdk/blob/7fbb08c491f9c8888cc0f3d86962acb3af672772/Release/samples/Oauth1Client/Oauth1Client.cpp
#include <iostream>
#include <cstdlib>
#include <sstream>

#ifdef __linux__
#include <cstring>
#elif defined(macintosh) || defined(Macintosh) || defined(__APPLE__) && defined(__MACH__)

#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>

#elif defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

#include <ShlObj.h>
#include <winbase.h>
#include <Shellapi.h>

#endif

#ifdef TEST_OPEN_BROWSER
#include "url.h"
#endif

// We could have used a plain string instead of
// creating a URL type, or we could use a url type
// from a library, but here we create our own
// for explanations sake.
static
void open_browser(const URL& url)
{
#ifdef __linux__
    // On linux xdg-open is a command that opens the
    // preferred application for the type of file or url.
    // For more information use: man xdg-open on the
    // terminal command line.
    //
    // In OAuth2 we open the browser for the user to
    // enter their credentials.
    std::string browser_cmd_string = static_cast<const std::ostringstream&>(
                                         std::ostringstream()
                                         << "xdg-open \""
                                         << url
                                         << '"'
                                         ).str();
    std::cout << browser_cmd_string << std::endl;
    (void)system(browser_cmd_string.c_str());

#else
    const std::string url_str = to_string(url);

#if defined(macintosh) || defined(Macintosh) || defined(__APPLE__) && defined(__MACH__)
    CFURLRef cf_url = CFURLCreateWithBytes (
            NULL,                        // allocator
            (UInt8*)url_str.c_str(),     // URLBytes
            (signed long)url_str.length(),            // length
            kCFStringEncodingASCII,      // encoding
            NULL                         // baseURL
    );
    LSOpenCFURLRef(cf_url,0);
    CFRelease(cf_url);
#elif defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    ShellExecuteA(NULL, "open", url_str.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

#endif
}

#ifdef TEST_OPEN_BROWSER
int main() {
    URL url("http://www.google.com");
    open_browser(url);
}
#endif

#endif /* OAUTH2_OPEN_BROWSER_H */
