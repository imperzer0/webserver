// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>

#ifndef FINEFTP_SERVER_CUSTOM_EVENT_HANDLER_HPP
#define FINEFTP_SERVER_CUSTOM_EVENT_HANDLER_HPP

#include "ftp_event_handler.h"


event_handler* ftp_handlers = new event_handler;

void add_custom_ftp_handler(ftp_event_handler_function handler)
{
	auto* it = ftp_handlers;
	for (; it->handler != nullptr && it->next != nullptr; it = it->next);

	if (it->handler != nullptr)
	{
		it->next = new event_handler { .next = nullptr, .handler = handler };
	}
	else
	{
		it->handler = handler;
	}
}

void execute_custom_ftp_handlers(
		const std::string& ftp_command, const std::string& parameters, const std::string& ftp_working_directory,
		std::shared_ptr<::fineftp::FtpUser> ftp_user
)
{
	for (auto* it = ftp_handlers; it != nullptr && it->handler != nullptr; it = it->next)
		it->handler(ftp_command, parameters, ftp_working_directory, ftp_user);
}

#endif //FINEFTP_SERVER_CUSTOM_EVENT_HANDLER_HPP
