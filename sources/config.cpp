// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.


#include <list>
#include <filesystem>
#include "server.h"
#include "config.h"
#include "../resources.hpp"
#include "constants.hpp"
#include "tools.h"

#ifdef ENABLE_FILESYSTEM_ACCESS
#include "../ftp/ftp_user.h"
#include "../ftp/ftp_event_handler.h"
#endif


#ifdef ENABLE_FILESYSTEM_ACCESS
typedef struct
{
	unsigned long long recent_uploads_count;
	std::list<std::pair<std::string, std::string>> recent_uploaded_files;
} dashboard_data;

static dashboard_data statistics = { .recent_uploads_count = 0, .recent_uploaded_files = { }};


typedef struct
{
	ftp_event_handler_function handler;
	std::string ftp_command;
	std::string parameters;
	std::string ftp_working_directory;
	std::shared_ptr<::fineftp::FtpUser> ftp_user;
} scheduled_handler;

static scheduled_handler* scheduled_action_command = nullptr;

void execute_next_time(
		ftp_event_handler_function handler, const std::string& ftp_command, const std::string& parameters,
		const std::string& ftp_working_directory, std::shared_ptr<::fineftp::FtpUser> ftp_user
)
{
	if (scheduled_action_command == nullptr)
		scheduled_action_command =
				new scheduled_handler { .handler = handler, .ftp_command = ftp_command, .parameters = parameters,
						.ftp_working_directory = ftp_working_directory, .ftp_user = ftp_user };
}
#endif


/// Register user-defined web path and ftp handlers
void register_additional_handlers()
{
	// TODO: You can create your own handlers over here
	// Template :
	// register_path_handler(
	// 		"PATH", "DESCRIPTION",
	// 		[](struct mg_connection* connection, struct mg_http_message* msg)
	// 		{
	//          **Your code Here**
	// 		}
	// );
	//
	// add_custom_ftp_handler(
	// 		[](const std::string& ftp_command, const std::string& parameters)
	// 		{
	//          **Your code Here**
	// 		}
	// );


#ifdef ENABLE_FILESYSTEM_ACCESS
	// Here is an exmple of a dashboard that shows statistics on uploaded files
	// implemented as web path and ftp handlers
	register_path_handler(
			"/dashboard", "View statistics on dashboard",
			[](struct mg_connection* connection, struct mg_http_message* msg) {
				std::string appendix;
				for (auto& f : statistics.recent_uploaded_files)
				{
					appendix += "<li><a href=\"/dir/" + f.second + "\">" + f.first + "</a></li>\n";
					MG_INFO(("Indexed '%s' => '%s'.", f.first.c_str(), f.second.c_str()));
				}

				mg_http_reply(
						connection, 200, "", RESOURCE(dashboard_html),
						statistics.recent_uploads_count, appendix.c_str()
				);
			}, registered_path_handler::STRICT
	);

	add_custom_ftp_handler(
			[](
					const std::string& ftp_command, const std::string& parameters, const std::string& ftp_working_directory,
					std::shared_ptr<::fineftp::FtpUser> ftp_user
			) {
				if (scheduled_action_command != nullptr)
				{
					scheduled_action_command->handler(
							scheduled_action_command->ftp_command, scheduled_action_command->parameters,
							scheduled_action_command->ftp_working_directory, scheduled_action_command->ftp_user
					);

					delete scheduled_action_command;
					scheduled_action_command = nullptr;
				}

				if (ftp_command == "STOR")
				{
					execute_next_time(
							[](
									const std::string& ftp_command, const std::string& parameters,
									const std::string& ftp_working_directory,
									std::shared_ptr<::fineftp::FtpUser> ftp_user
							) {
								std::string filepath;
								if (!parameters.empty() && (parameters[0] == '/'))
									filepath = parameters;
								else
									filepath = ftp_working_directory + "/" + parameters;

								std::string local_root_path = ftp_user->local_root_path_;
								while (local_root_path.ends_with('/')) local_root_path.pop_back();
								filepath = local_root_path + "/" + filepath;

								struct stat st { };
								if (::stat(filepath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
								{
									++statistics.recent_uploads_count;

									filepath.erase(filepath.size() - 5); // remove .part additional extension
									std::filesystem::path base(getcwd());
									std::filesystem::path fpath(filepath);
									statistics.recent_uploaded_files.push_front(
											{ path_basename(filepath), std::filesystem::relative(fpath, base).string() }
									);
									while (statistics.recent_uploaded_files.size() > MAX_RECENT_UPLOAD_RECORDS_COUNT)
										statistics.recent_uploaded_files.pop_back();
								}
							}, ftp_command, parameters, ftp_working_directory, std::move(ftp_user)
					);
				}
			}
	);
#endif
}


///
/// TODO: You can add email hostnames to one of this lists to filter out undesired email hosts
/// Note: If you leave the list empty it will not take any effect
///

std::set<std::string> server_verification_email_hosts_whitelist {
		// Add hostnames here as comma separated list of strings: "str1", "str2", "str3" ... "strn"
};

std::set<std::string> server_verification_email_hosts_blacklist {
		// Add hostnames here as comma separated list of strings: "str1", "str2", "str3" ... "strn"
};