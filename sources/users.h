/// Most methods related to user account management

#ifndef USERS_H
#define USERS_H

#include "settings.h"
#include <map>
#include <fineftp/server.h>

typedef id_t uint32_t;
typedef std::map<std::string, std::pair<std::string, std::string>> __user_map_t;
typedef std::map<id_t, __user_map_t::value_type> __pending_user_map_t;


/// Get a map of registered users
extern const __user_map_t* get_registered_users();

// Load user credentials from passwd file
extern bool load_users();

// Save user credentials to passwd file
extern bool save_users();

// Add new user and save all
extern bool add_new_user(const __user_map_t::value_type& user_data);


#ifdef ENABLE_FILESYSTEM_ACCESS

// Add all current users to ftp_server instance
extern void forward_users(fineftp::FtpServer& ftp_server);

// Add a user to ftp_server
extern void add_user(fineftp::FtpServer& ftp_server, const __user_map_t::value_type& reg_user);

#endif


extern bool pending_id_exists(id_t id);
extern bool add_pending_user(id_t id, const __user_map_t::value_type& user);
extern __pending_user_map_t::iterator find_pending_user(id_t id);
extern __pending_user_map_t::iterator pending_user_invalid();

#endif //USERS_H
