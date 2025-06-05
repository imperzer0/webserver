/// Most methods related to user account management

#include "users.h"


#include "constants.h"
#include "tools.h"
#include "../mongoose/mongoose.h"

#include <cstdio>
#include <map>
#include <string>
#include <bits/local_lim.h>


// Users: login, email, password
static __user_map_t registered_users;
// Users who just created their account and need to verify their email address
static __pending_user_map_t registered_users_pending;


const __user_map_t* get_registered_users()
{
    return &registered_users;
}

bool load_users()
{
    MG_DEBUG(("[USERS] Loading users from file[" CONFIG_DIR "passwd" "]..."));
    FILE* file = ::fopen(CONFIG_DIR "passwd", "rb"); // Open users file
    if (file)
    {
        bool success = true;
        while (!::feof(file))
        {
            char username[HOST_NAME_MAX + 1]{};
            char email[HOST_NAME_MAX + 1]{};
            char password[HOST_NAME_MAX + 1]{};
            if (::fscanf(file, "%s : %s : %s\n", username, email, password) == 3) // Scan line in format "<user> : <email> : <password>\n"
                // Store user's credentials in a map
                success = success && registered_users.insert({username, {email, password}}).second;
        }
        ::fclose(file);
        return success;
    }
    return false;
}

bool save_users()
{
    FILE* file = ::fopen(CONFIG_DIR "passwd", "wb");
    if (!file)
        return false;
    for (auto& reg_user : registered_users)
        ::fprintf(
            file, "%s : %s : %s\n",
            reg_user.first.c_str(), reg_user.second.first.c_str(), reg_user.second.second.c_str()
        );
    ::fclose(file);
    return true;
}


bool add_new_user(const __user_map_t::value_type& user_data)
{
    auto user = registered_users.insert(user_data);
    return user.second && save_users();
}


#ifdef ENABLE_FILESYSTEM_ACCESS

void forward_users(fineftp::FtpServer& ftp_server)
{
    std::string cwd(getcwd());
    cwd += '/';

    for (const auto& reg_user : registered_users)
    {
        std::string root_dir = cwd + reg_user.first;
        if (mkdir_p(root_dir))
        {
            MG_DEBUG(("[FTP] Adding user \"%s\" to ftp server...", reg_user.first.c_str()));
            ftp_server.addUser(reg_user.first, reg_user.second.second, root_dir, fineftp::Permission::All);
        }
    }
}

void add_user(fineftp::FtpServer& ftp_server, const __user_map_t::value_type& reg_user)
{
    std::string root_dir = getcwd() + '/' + reg_user.first;
    if (mkdir_p(root_dir))
    {
        MG_DEBUG(("[FTP] Adding user \"%s\" to ftp server...", reg_user.first.c_str()));
        ftp_server.addUser(reg_user.first, reg_user.second.second, root_dir, fineftp::Permission::All);
    }
}

#endif


bool pending_id_exists(id_t id)
{
    return registered_users_pending.contains(id);
}

bool add_pending_user(id_t id, const __user_map_t::value_type& user)
{
    return registered_users_pending.insert({id, user}).second;
}

__pending_user_map_t::iterator find_pending_user(id_t id)
{
    return registered_users_pending.find(id);
}

__pending_user_map_t::iterator pending_user_invalid()
{
    return registered_users_pending.end();
}
