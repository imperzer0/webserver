// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>

/// Configurations and settings that generally don't break the project.
/// Therefore, they are called settings

#ifndef WEBSERVER_SETTINGS_H
#define WEBSERVER_SETTINGS_H

#include <set>
#include <string>


/* Un/Comment this macro to Enable/Disable ftp server and access to the filesystem on the server */
#define ENABLE_FILESYSTEM_ACCESS

/* Display time in a human-readable way,   not just as %ul */
#define MG_HTTP_DIRLIST_TIME_FMT


#define COLOR_400 "rgba(147, 0, 0, 0.90)"
#define COLOR_401 "rgba(147, 51, 0, 0.90)"
#define COLOR_403 "rgba(147, 83, 0, 0.90)"
#define COLOR_404 "rgba(147, 122, 0, 0.90)"
#define COLOR_405 "rgba(147, 147, 0, 0.90)"
#define COLOR_406 "rgba(0, 147, 125, 0.90)"
#define COLOR_409 "rgba(47, 0, 147, 0.90)"
#define COLOR_500 "rgba(147, 0, 56, 0.90)"
#define COLOR_501 "rgba(147, 0, 100, 0.90)"
#define COLOR_503 "rgba(147, 0, 142, 0.90)"

/// 400 Bad Request <br>
/// 401 Unauthorized <br>
/// 403 Forbidden <br>
/// 404 Not Found <br>
/// 405 Method Not Allowed <br>
/// 406 Not Acceptable <br>
/// 409 Conflict <br>
/// 500 Internal Server Error <br>
/// 501 Not Implemented <br>
/// 503 Service Unavailable <br>
#define COLORED_ERROR(color) color, COLOR_##color


extern std::set<std::string> server_verification_email_hosts_whitelist;
extern std::set<std::string> server_verification_email_hosts_blacklist;

#ifdef ENABLE_FILESYSTEM_ACCESS
extern void register_additional_handlers();
#endif

# ifndef MAX_RECENT_UPLOAD_RECORDS_COUNT
#  define MAX_RECENT_UPLOAD_RECORDS_COUNT 20 // Used in the custom handlers demo
# endif

# ifndef REGEX_LOGIN
#  define REGEX_LOGIN "^[a-zA-Z0-9_]*$" // Allowed Login format (REGEX)
# endif

# ifndef REGEX_EMAIL
#  define REGEX_EMAIL R"(^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+$)" // Allowed Email format (REGEX)
# endif

#endif //WEBSERVER_SETTINGS_H
