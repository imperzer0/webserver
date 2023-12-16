// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.


#ifndef WEBSERVER_SERVER_H
#define WEBSERVER_SERVER_H

#include <string>
#include <map>
#include "../mongoose.h"

#ifdef ENABLE_FILESYSTEM_ACCESS
#include "../ftp/ftp_event_handler.h"
#endif


extern const char* http_address;
extern const char* https_address;
extern const char* tls_path;
extern const char* server_confirmator_email;
extern const char* server_confirmator_email_password;
extern const char* server_confirmator_smtp_server;
extern int log_level, hexdump;

#ifdef ENABLE_FILESYSTEM_ACCESS
extern pthread_mutex_t ftp_callback_mutex;
#endif

extern void server_initialize();

extern void server_run();


typedef void (* path_handler_function)(struct mg_connection* connection, struct mg_http_message* msg);

typedef struct
{
	std::string path;
	std::string description;
	path_handler_function fn;
	enum : int { STRICT = 0, SOFT } restriction_type;
} registered_path_handler;

extern void register_path_handler(
		const std::string& path, const std::string& description, path_handler_function fn,
		decltype(registered_path_handler::restriction_type) type
);

extern const auto* get_registered_users();

#endif //WEBSERVER_SERVER_H
