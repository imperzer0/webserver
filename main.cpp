// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be agreed with the author of this comment.

#include "server.h"
#include "constants.hpp"
#include <getopt.h>


static constexpr const char* short_args = "a:l:Hvh";
static constexpr struct option long_args[] = {
		{ "address",  required_argument, nullptr, 'a' },
		{ "loglevel", required_argument, nullptr, 'l' },
		{ "hexdump",  no_argument,       nullptr, 'H' },
		{ "version",  no_argument,       nullptr, 'v' },
		{ "help",     no_argument,       nullptr, 'h' },
		{ nullptr, 0,                    nullptr, 0 }
};


inline static void help()
{
	::printf(APPNAME " v" VERSION "\n");
	::printf("Usage: " APPNAME " [OPTIONS]...\n");
	::printf("Runs http server.\n");
	::printf("Options:\n");
	::printf(" --address    | a  <address>   Listening address. Default: %s\n", address);
	::printf(" --loglevel   | l  <level>     Set log level (from 0 to 4). Default: %d\n", log_level);
	::printf(" --hexdump    | H              Enable hex dump.\n");
	::printf(" --version    | v              Show version information.\n");
	::printf(" --help       | h              Show this help message.\n");
	::printf("\n");
	
	::exit(34);
}

int main(int argc, char** argv)
{
	int option_index, option, destroy_db = 0;
	while ((option = ::getopt_long(argc, argv, short_args, long_args, &option_index)) > 0)
	{
		switch (option)
		{
			case 'a':
				address = ::strdup(optarg);
				break;
			case 'l':
				log_level = ::strtol(::strdup(optarg), nullptr, 10);
				break;
			case 'H':
				hexdump = 1;
				break;
			case 'v':
			{
				::printf(APPNAME "version: " VERSION "\n");
				help();
				break;
			}
			case 'h':
			{
				help();
				break;
			}
			default:
				help();
		}
	}
	
	server_initialize();
	server_run();
	
	return 0;
}
