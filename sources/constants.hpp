//
// Created by imper on 8/16/22.
//

#ifndef WEBSERVER_CONSTANTS_HPP
#define WEBSERVER_CONSTANTS_HPP


# define _STR(s) #s
# define MACRO_STR(v) _STR(v)


# ifndef VERSION
#  define VERSION "(devel)"
# endif

# ifndef APPNAME
#  define APPNAME "webserver"
# endif

# ifndef DEFAULT_HTTP_SERVER_ADDRESS
#  define DEFAULT_HTTP_SERVER_ADDRESS "http://0.0.0.0:80"
# endif

# ifndef DEFAULT_HTTPS_SERVER_ADDRESS
#  define DEFAULT_HTTPS_SERVER_ADDRESS "https://0.0.0.0:443"
# endif

# ifndef MAX_INLINE_FILE_SIZE
#  define MAX_INLINE_FILE_SIZE 16777216 // 16 MB
# endif

# ifndef MAX_RECENT_UPLOAD_RECORDS_COUNT
#  define MAX_RECENT_UPLOAD_RECORDS_COUNT 20
# endif


#endif //WEBSERVER_CONSTANTS_HPP
