// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be agreed with the author of this comment.


#ifndef WEBSERVER_SERVER_H
#define WEBSERVER_SERVER_H

#include "mongoose.h"


extern const char* address;
extern int log_level, hexdump;

extern void server_initialize();

extern void server_run();

#endif //WEBSERVER_SERVER_H
