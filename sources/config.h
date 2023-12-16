// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.


#ifndef WEBSERVER_CONFIG_H
#define WEBSERVER_CONFIG_H

/* Uncomment this macro to enable ftp server */
//#define ENABLE_FILESYSTEM_ACCESS


#ifdef ENABLE_FILESYSTEM_ACCESS
extern void register_additional_handlers();
#endif

#endif //WEBSERVER_CONFIG_H
