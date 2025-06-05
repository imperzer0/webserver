/// Most methods related to user account management

#ifndef USERS_H
#define USERS_H

#include "settings.h"
#include <map>
#include <fineftp/server.h>

typedef id_t uint32_t;
typedef std::map<std::string, std::pair<std::string, std::string>> __user_map_t;
typedef std::map<id_t, __user_map_t::value_type> __pending_user_map_t;


extern const auto* get_registered_users();

extern bool load_users();
extern bool save_users();
extern bool add_new_user(const __user_map_t::value_type& user_data);

#ifdef ENABLE_FILESYSTEM_ACCESS

extern void forward_users(fineftp::FtpServer& ftp_server);
extern void add_user(fineftp::FtpServer& ftp_server, const __user_map_t::value_type& reg_user);

#endif


extern bool pending_id_exists(id_t id);
extern void add_pending_user(id_t id, const __user_map_t::value_type& user);
extern __pending_user_map_t::iterator find_pending_user(id_t id);
extern __pending_user_map_t::iterator pending_user_invalid();

#endif //USERS_H
