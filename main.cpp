// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be agreed with the author of this comment.

#include "server.h"
#include "constants.hpp"
#include <getopt.h>


static constexpr const char* short_args = "t:l:m:M:s:Hvh";
static constexpr struct option long_args[] = {
		{ "http_address",   required_argument, nullptr, 10 },
		{ "https_address",  required_argument, nullptr, 11 },
		{ "tls",            required_argument, nullptr, 't' },
		{ "loglevel",       required_argument, nullptr, 'l' },
		{ "email",          required_argument, nullptr, 'm' },
		{ "email-password", required_argument, nullptr, 'M' },
		{ "smtp-server",    required_argument, nullptr, 's' },
		{ "hexdump",        no_argument,       nullptr, 'H' },
		{ "version",        no_argument,       nullptr, 'v' },
		{ "help",           no_argument,       nullptr, 'h' },
		{ nullptr, 0,                          nullptr, 0 }
};


inline static void help()
{
	::printf(APPNAME " v" VERSION "\n");
	::printf("Usage: " APPNAME " [OPTIONS]...\n");
	::printf("Runs http server.\n");
	::printf("Options:\n");
	::printf(" --http_address    |    <ip address>  Listening http_address. Default: %s\n", http_address);
	::printf(" --https_address   |    <ip address>  Listening https_address. Default: %s\n", https_address);
	::printf(" --tls             | t  <path>        Path to ssl keypair directory. If not specified - no tls.\n");
	::printf(" --loglevel        | l  <level>       Set log level (from 0 to 4). Default: %d\n", log_level);
	::printf(" --email           | m  <email>       Email for user registration confirmation.\n");
	::printf(" --email-password  | M  <password>    Email's account password.\n");
	::printf(" --smtp-server     | s  <ip address>  SMTP server api address.\n");
	::printf(" --hexdump         | H                Enable hex dump.\n");
	::printf(" --version         | v                Show version information.\n");
	::printf(" --help            | h                Show this help message.\n\n");
	::printf("* NOTE: To generate ssl certificates in specific folder run commands:\n");
	::printf("CASUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CAwebserver\";\n");
	::printf("CRTSUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CRTwebserver\";\n");
	::printf("# Generate CA (Certificate Authority)\n");
	::printf("openssl genrsa -out ca.key 2048;\n");
	::printf("openssl req -new -x509 -days 365 -key ca.key -out ca.pem -subj $CASUBJ;\n");
	::printf("# Generate server certificate\n");
	::printf("openssl genrsa -out key.pem 2048;\n");
	::printf("openssl req -new -key key.pem -out csr.pem -subj $CRTSUBJ;\n");
	::printf("openssl x509 -req -days 365 -in csr.pem -CA ca.pem -CAkey ca.key -set_serial 01 -out cert.pem;\n");
	
	::exit(34);
}

int main(int argc, char** argv)
{
	int option_index, option;
	while ((option = ::getopt_long(argc, argv, short_args, long_args, &option_index)) > 0)
	{
		switch (option)
		{
			case 10:
				http_address = ::strdup(optarg);
				break;
			case 11:
				https_address = ::strdup(optarg);
				break;
			case 't':
				tls_path = ::strdup(optarg);
				break;
			case 'l':
				log_level = ::strtol(optarg, nullptr, 10);
				break;
			case 'm':
				server_confirmator_email = strdup(optarg);
				break;
			case 'M':
				server_confirmator_email_password = strdup(optarg);
				break;
			case 's':
				server_confirmator_smtp_server = strdup(optarg);
				break;
			case 'H':
				hexdump = 1;
				break;
			case 'v':
				::printf(APPNAME "version: " VERSION "\n");
				help();
				break;
			case 'h':
				help();
				break;
			default:
				help();
		}
	}
	
	server_initialize();
	server_run();
	
	return 0;
}
