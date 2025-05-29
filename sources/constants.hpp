/// Project-wide constants. Make changes with caution!

#ifndef WEBSERVER_CONSTANTS_HPP
#define WEBSERVER_CONSTANTS_HPP


# define _STR(s) #s
# define MACRO_STR(v) _STR(v)


// The version is defined by cmake when you run a script that makes
// a debian or arch package. Otherwise - it is set to "(dev)" below.
# ifndef VERSION
#  define VERSION "(dev)"
# endif

# ifndef APPNAME
#  define APPNAME "webserver"
# endif

# ifndef DEFAULT_HTTP_SERVER_ADDRESS
#  define DEFAULT_HTTP_SERVER_ADDRESS "http://0.0.0.0:80" // Accept all on port 80
# endif

# ifndef DEFAULT_HTTPS_SERVER_ADDRESS
#  define DEFAULT_HTTPS_SERVER_ADDRESS "https://0.0.0.0:443" // Accept all on port 443
# endif

# ifndef MAX_INLINE_FILE_SIZE
#  define MAX_INLINE_FILE_SIZE 16777216 // 16 MB
# endif


#endif //WEBSERVER_CONSTANTS_HPP
