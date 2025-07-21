// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>

#include "ftp_event_handler.h"


template <typename Fn_>
ftp_injected<Fn_>& ftp_injected<Fn_>::$() { return *_this; }

template <typename Fn_>
template <typename... Args>
void ftp_injected<Fn_>::call_this_one(Args... args)
{
    if (handler)
        return handler(args...);
}

template <typename Fn_>
template <typename... Args>
void ftp_injected<Fn_>::operator()(Args... args)
{
    for (auto* it = this; it != nullptr && it->handler != nullptr; it = it->next)
        it->call_this_one(args...);
}

template <typename Fn_>
void ftp_injected<Fn_>::add(Fn_ fn)
{
    auto* it = this->end;
    for (; it->handler != nullptr && it->next != nullptr; it = it->next) { }

    if (it->handler != nullptr)
        it->next = new ftp_injected(fn);
    else
        it->handler = fn;
    this->end = it;
}

template <typename Fn_>
ftp_injected<Fn_>::ftp_injected() = default;

template <typename Fn_>
ftp_injected<Fn_>::ftp_injected(Fn_ fn) : handler(fn) { }

template <typename Fn_>
ftp_injected<Fn_>* ftp_injected<Fn_>::_this = new ftp_injected();


/// Tell TU to generate classes for the following functions:
template class ftp_injected<on_send_fn>; // on_receive is the same
template class ftp_injected<on_process_fn>;
