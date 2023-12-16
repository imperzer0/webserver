// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be agreed with the author of this comment.


#ifndef WEBSERVER_SERVER_H
#define WEBSERVER_SERVER_H

#include <string>
#include "../mongoose.h"
#include "../ftp/ftp_event_handler.h"


extern const char* http_address;
extern const char* https_address;
extern const char* tls_path;
extern const char* server_confirmator_email;
extern const char* server_confirmator_email_password;
extern const char* server_confirmator_smtp_server;
extern int log_level, hexdump;

extern pthread_mutex_t ftp_callback_mutex;

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

#endif //WEBSERVER_SERVER_H
