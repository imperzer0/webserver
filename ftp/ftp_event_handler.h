// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>

#ifndef FINEFTP_SERVER_FTP_EVENT_HANDLER_H
#define FINEFTP_SERVER_FTP_EVENT_HANDLER_H

#include <string>
#include <memory>
#include "ftp_user.h"

typedef void (*on_send_fn)(const std::string& raw_message);
typedef void (*on_receive_fn)(const std::string& packet_string);
typedef void (*on_process_fn)(
        const std::string& ftp_command, const std::string& parameters,
        const std::string& ftp_working_directory, std::shared_ptr<::fineftp::FtpUser> ftp_user
    );

template <typename Fn_>
class ftp_injected
{
public:
    static ftp_injected& $();

    template <typename... Args>
    void operator()(Args... args);

    template <typename... Args>
    void call_this_one(Args... args);

    void add(Fn_ fn);

protected:
    ftp_injected();
    ftp_injected(Fn_ fn);

private:
    static ftp_injected* _this;

    ftp_injected* next = nullptr;
    ftp_injected* end = this;
    Fn_ handler = nullptr;
};

/// Promise TU to generate classes later
extern template class ftp_injected<on_send_fn>; // on_receive is the same
extern template class ftp_injected<on_process_fn>;

#endif //FINEFTP_SERVER_FTP_EVENT_HANDLER_H
