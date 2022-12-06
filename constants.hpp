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

# ifndef DEFAULT_SERVER_ADDRESS
#  define DEFAULT_SERVER_ADDRESS "http://0.0.0.0:80"
# endif

# ifndef MAX_INLINE_FILE_SIZE
#  define MAX_INLINE_FILE_SIZE 16777216 // 16 MB
# endif


#endif //WEBSERVER_CONSTANTS_HPP
