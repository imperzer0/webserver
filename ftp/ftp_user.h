//
// Created by imper on 12/18/22.
//

#ifndef WEBSERVER_FTP_USER_H
#define WEBSERVER_FTP_USER_H

#include <fineftp/permissions.h>


namespace fineftp
{
	struct FtpUser
	{
		FtpUser(const std::string& password, const std::string& local_root_path, const Permission permissions)
				: password_(password), local_root_path_(local_root_path), permissions_(permissions) { }

		const std::string password_;
		const std::string local_root_path_;
		const Permission permissions_;
	};
}

#endif //WEBSERVER_FTP_USER_H
