// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>

#ifndef FINEFTP_SERVER_FTP_EVENT_HANDLER_H
#define FINEFTP_SERVER_FTP_EVENT_HANDLER_H

#include <string>
#include <memory>
#include "ftp_user.h"


typedef void
(* ftp_event_handler_function)(
		const std::string& ftp_command, const std::string& parameters, const std::string& ftp_working_directory,
		std::shared_ptr<::fineftp::FtpUser> ftp_user
);

typedef struct event_handler
{
	struct event_handler* next = nullptr;
	ftp_event_handler_function handler = nullptr;
} event_handler;

extern event_handler* ftp_handlers;

extern void add_custom_ftp_handler(ftp_event_handler_function handler);

#endif //FINEFTP_SERVER_FTP_EVENT_HANDLER_H
