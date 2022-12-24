// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be agreed with the author of this comment.


#include "server.h"
#include "mongoose.c"
#include "constants.hpp"
#include "strscan.c"
#include "resources.hpp"
#include "config.h"
#include <fineftp/server.h>
#include <map>
#include <ftw.h>
#include <curl/curl.h>


typedef id_t uint32_t;

const char* http_address = DEFAULT_HTTP_SERVER_ADDRESS;
const char* https_address = DEFAULT_HTTPS_SERVER_ADDRESS;
const char* tls_path = nullptr;
const char* server_confirmator_email = nullptr;
const char* server_confirmator_email_password = nullptr;
const char* server_confirmator_smtp_server = "smtps://smtp.gmail.com:465";
int log_level = 2, hexdump = 0;

pthread_mutex_t ftp_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct mg_mgr manager{ };
static struct mg_connection* http_server_connection = nullptr, * https_server_connection = nullptr;
static fineftp::FtpServer ftp_server;
static std::map<std::string, std::pair<std::string, std::string>> ftp_users;
static std::map<id_t, std::pair<std::string, std::pair<std::string, std::string>>> ftp_users_pending;

// Handle interrupts, like Ctrl-C
static int s_signo = 0;

static void signal_handler(int signo)
{
	MG_ERROR(("[SIGNAL_HANDLER] Captured SIG%s(%d) \"%s\".", sigabbrev_np(signo), signo, sigdescr_np(signo)));
	signal(signo, SIG_DFL);
	s_signo = signo;
}


typedef struct __registered_path_handlers
{
	registered_path_handler* data = nullptr;
	struct __registered_path_handlers* next = nullptr;
} registered_path_handlers;

static registered_path_handlers* handlers = nullptr;
static registered_path_handlers* handlers_head = nullptr;


mutex_locker::mutex_locker(pthread_mutex_t* mutex) : mutex(mutex)
{
	pthread_mutex_lock(mutex);
}

mutex_locker::~mutex_locker()
{
	pthread_mutex_unlock(mutex);
}


/// Return true if str starts with prefix
inline bool starts_with(const char* str, const char* prefix);

/// Get current working directory
inline std::string getcwd();

/// Erase all seq occurrences in str
inline std::string erase_all(const std::string& str, const std::string& seq);

/// Remove .. subdirectories from path to prevent sandbox escape
inline std::string secure_path(const std::string& path);

/// Get parent directory name of entry at given path
inline std::string path_dirname(const std::string& path);

/// Get entry base name
inline std::string path_basename(std::string path);

/// rm -rf
inline bool rm_rf(const std::string& path);

/// mkdir -p
inline bool mkdir_p(const std::string& path);

/// Load ftp users from file
inline static void load_users();

/// Save ftp users into file
inline static void save_users();

/// Create users on ftp server
inline static void forward_all_users();

/// Create user on ftp server
inline static void add_user(decltype(*ftp_users.begin())& ftp_user);


/// Send icon resource string as regular file over http
inline void http_send_resource(
		struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize, const char* mime_type
);

/// Send error page over http
inline void send_error_html(struct mg_connection* connection, int code, const char* color);


/// Handle index page access
inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle favicon access
inline void handle_favicon_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle filesystem access (serve directory)
inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user registration form access
inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user registration request
inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user email confirmation request
inline void handle_confirm_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle other resources request
inline void handle_resources_html(struct mg_connection* connection, struct mg_http_message* msg);



/// Add path handler to global linked list
void register_path_handler(
		const std::string& path, const std::string& description, path_handler_function fn,
		decltype(registered_path_handler::restriction_type) type
)
{
	MG_INFO(("Registering '%s' => '%s' path...", description.c_str(), path.c_str()));
	if (!handlers_head)
	{
		handlers = new registered_path_handlers{
				.data = new registered_path_handler{ .path = path, .description = description, .fn = fn, .restriction_type = type },
				.next = nullptr };
		handlers_head = handlers;
	}
	else
	{
		handlers_head->next = new registered_path_handlers{
				.data = new registered_path_handler{ .path = path, .description = description, .fn = fn, .restriction_type = type },
				.next = nullptr };
		handlers_head = handlers_head->next;
	}
}

/// Iterate through registered handlers and try handle them
inline void handle_registered_paths(struct mg_connection* connection, struct mg_http_message* msg)
{
	MG_INFO(("Handling non-builtin registered paths..."));
	registered_path_handlers* root = handlers;
	
	while (root && root->data)
	{
		switch (root->data->restriction_type)
		{
			case registered_path_handler::STRICT:
				if (mg_http_match_uri(msg, root->data->path.c_str()))
					goto exit_loop;
				break;
			case registered_path_handler::SOFT:
				if (starts_with(msg->uri.ptr, root->data->path.c_str()))
					goto exit_loop;
				break;
		}
		root = root->next;
	}
	
	exit_loop:
	
	if (root && root->data)
	{
		char addr[20];
		mg_ntoa(&connection->rem, addr, sizeof addr);
		MG_INFO(("Handling '%s' => '%s' path for IP %s...", root->data->description.c_str(), root->data->path.c_str(), addr));
		return root->data->fn(connection, msg);
	}
	
	send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
}



inline void handle_http_message(struct mg_connection* connection, struct mg_http_message* msg)
{
	if (mg_http_match_uri(msg, "/") || mg_http_match_uri(msg, "/index.html") || mg_http_match_uri(msg, "/index"))
		handle_index_html(connection, msg);
	else if (mg_http_match_uri(msg, "/favicon.ico") || mg_http_match_uri(msg, "/favicon"))
		handle_favicon_html(connection, msg);
	else if (starts_with(msg->uri.ptr, "/dir/"))
		handle_dir_html(connection, msg);
	else if (mg_http_match_uri(msg, "/register-form"))
		handle_register_form_html(connection, msg);
	else if (mg_http_match_uri(msg, "/register"))
		handle_register_html(connection, msg);
	else if (starts_with(msg->uri.ptr, "/confirm/"))
		handle_confirm_html(connection, msg);
	else if (starts_with(msg->uri.ptr, "/resources/"))
		handle_resources_html(connection, msg);
	else handle_registered_paths(connection, msg);
}



/// Handle mongoose events
void client_handler(struct mg_connection* connection, int ev, void* ev_data, void* fn_data)
{
	if (fn_data != nullptr && ev == MG_EV_ACCEPT)
	{
		std::string stls_path(tls_path);
		if (!stls_path.ends_with('/')) stls_path += '/';
		
		std::string cert_path(stls_path);
		cert_path += "cert.pem";
		
		std::string key_path(stls_path);
		key_path += "key.pem";
		
		struct mg_tls_opts opts = {
				.cert = cert_path.c_str(),   // Certificate PEM file
				.certkey = key_path.c_str(), // This pem contains both cert and key
		};
		mg_tls_init(connection, &opts);
	}
	else if (ev == MG_EV_HTTP_MSG)
	{
		auto* msg = static_cast<mg_http_message*>(ev_data);
		handle_http_message(connection, msg);
	}
}

/// Initialize server
void server_initialize()
{
	if (server_confirmator_email != nullptr && server_confirmator_email_password == nullptr)
	{
		puts("[Server] Error occurred in server initialization: Confirmator email's password was not specified.");
		exit(-3);
	}
	
	if (server_confirmator_email == nullptr || *server_confirmator_email == 0)
	{
		puts("[Server] ======= Important information ======");
		puts("[Server] Confirmator email was not specified. Entering unsafe mode!!!");
		puts("[Server] !!! Pay attention! Your server could be vulnerable to spam attack! !!!");
		puts("[Server] Please, consider creating a google account and setup it for email verification.");
		puts("[Server] ======= Important information ======");
		delete[] server_confirmator_email;
		delete[] server_confirmator_email_password;
		server_confirmator_email = server_confirmator_email_password = nullptr;
	}
	
	// Initialize the libcurl library
	curl_global_init(CURL_GLOBAL_ALL);
	
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGQUIT, signal_handler);
	
	mg_log_set(log_level);
	mg_mgr_init(&manager);
	
	if (log_level >= 2)
	{
		add_custom_ftp_handler(
				[](
						const std::string& ftp_command, const std::string& parameters, const std::string& ftp_working_directory,
						std::shared_ptr<::fineftp::FtpUser> ftp_user
				)
				{
					mutex_locker l(&ftp_callback_mutex);
					MG_INFO((" [FTP] %s %s", ftp_command.c_str(), parameters.c_str()));
				}
		);
	}
	
	register_additional_handlers();
	
	MG_INFO(("[FTP] Adding anonymous user to ftp server..."));
	ftp_server.addUserAnonymous(getcwd(), fineftp::Permission::ReadOnly);
	
	MG_INFO(("[FTP] Loading users from file..."));
	load_users();
	MG_INFO(("[FTP] Forwarding users to ftp server..."));
	forward_all_users();
}

/// Start listening on given http_address and run server loop
void server_run()
{
	if (!(http_server_connection = mg_http_listen(&manager, http_address, client_handler, nullptr)))
	{
		MG_ERROR(("Cannot start listening on %s. Use 'http://ADDR:PORT' or just ':PORT' as http address parameter", http_address));
		exit(EXIT_FAILURE);
	}
	
	if (tls_path)
	{
		if (!(https_server_connection = mg_http_listen(&manager, https_address, client_handler, (void*)1)))
		{
			MG_ERROR(("Cannot start listening on %s. Use 'https://ADDR:PORT' or just ':PORT' as https address parameter", https_address));
			https_server_connection = nullptr;
		}
	}
	
	if (hexdump)
		http_server_connection->is_hexdumping = 1;
	
	if (hexdump && https_server_connection)
		https_server_connection->is_hexdumping = 1;
	
	auto cwd = getcwd();
	
	MG_INFO((""));
	MG_INFO(("Mongoose v" MG_VERSION));
	MG_INFO(("Server listening on : [%s]", http_address));
	MG_INFO(("Web root directory  : [file://%s/]", cwd.c_str()));
	MG_INFO((""));
	
	if (https_server_connection)
	{
		MG_INFO((""));
		MG_INFO(("Mongoose v" MG_VERSION));
		MG_INFO(("Server listening on : [%s]", https_address));
		MG_INFO(("Web root directory  : [file://%s/]", cwd.c_str()));
		MG_INFO((""));
	}
	
	ftp_server.start(4);
	MG_INFO(("[FTP]"));
	MG_INFO(("[FTP] Started ftp server on : [ftp://%s:%d]", ftp_server.getAddress().c_str(), ftp_server.getPort()));
	MG_INFO(("[FTP] Web root directory    : [file://%s/]", cwd.c_str()));
	MG_INFO(("[FTP]"));
	
	while (s_signo == 0) mg_mgr_poll(&manager, 1000);
	
	mg_mgr_free(&manager);
	ftp_server.stop();
	MG_INFO(("Exiting due to signal [%d]...", s_signo));
	
	exit(s_signo);
}



inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Serving index.html to %s...", addr));
	size_t accumulate = 0, count = 0;
	std::string appendix;
	for (auto* i = handlers; i != nullptr && i->data != nullptr;
	     accumulate += i->data->path.size() + i->data->description.size(), ++count, i = i->next)
	{
		appendix += "<li><a href=\"" + i->data->path + "\">" + i->data->description + "</a></li>\n";
		MG_INFO(("Indexed '%s' => '%s'.", i->data->description.c_str(), i->data->path.c_str()));
	}
	
	char article_complete[LEN(article_html) + count * 20 + accumulate + 1];
	sprintf(article_complete, RESOURCE(article_html), appendix.c_str());
	
	mg_http_reply(
			connection, 200, "Content-Type: text/html\r\n",
			RESOURCE(index_html), article_complete
	);
}


inline void handle_favicon_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Serving favicon.ico to %s...", addr));
	http_send_resource(connection, msg, RESOURCE(favicon_ico), LEN(favicon_ico), "image/x-icon");
}


static void list_dir(struct mg_connection* c, struct mg_http_message* hm, const struct mg_http_serve_opts* opts, char* dir)
{
	const char* sort_js_code =
			"<script>function srt(tb, sc, so, d) {"
			"var tr = Array.prototype.slice.call(tb.rows, 0),"
			"tr = tr.sort(function (a, b) { var c1 = a.cells[sc], c2 = b.cells[sc],"
			"n1 = c1.getAttribute('name'), n2 = c2.getAttribute('name'), "
			"t1 = a.cells[2].getAttribute('name'), "
			"t2 = b.cells[2].getAttribute('name'); "
			"return so * (t1 < 0 && t2 >= 0 ? -1 : t2 < 0 && t1 >= 0 ? 1 : "
			"n1 ? parseInt(n2) - parseInt(n1) : "
			"c1.textContent.trim().localeCompare(c2.textContent.trim())); });";
	const char* sort_js_code2 =
			"for (var i = 0; i < tr.length; i++) tb.appendChild(tr[i]); "
			"if (!d) window.location.hash = ('sc=' + sc + '&so=' + so); "
			"};"
			"window.onload = function() {"
			"var tb = document.getElementById('tb');"
			"var m = /sc=([012]).so=(1|-1)/.exec(window.location.hash) || [0, 2, 1];"
			"var sc = m[1], so = m[2]; document.onclick = function(ev) { "
			"var c = ev.target.rel; if (c) {if (c == sc) so *= -1; srt(tb, c, so); "
			"sc = c; ev.preventDefault();}};"
			"srt(tb, sc, so, true);"
			"}"
			"</script>";
	struct mg_fs* fs = opts->fs == nullptr ? &mg_fs_posix : opts->fs;
	struct printdirentrydata d = { c, hm, opts, dir };
	char tmp[10], buf[MG_PATH_MAX];
	size_t off, n;
	int len = mg_url_decode(hm->uri.ptr, hm->uri.len, buf, sizeof(buf), 0);
	struct mg_str uri = len > 0 ? mg_str_n(buf, (size_t)len) : hm->uri;
	
	mg_printf(
			c,
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"%s"
			"Content-Length:         \r\n\r\n",
			opts->extra_headers == nullptr ? "" : opts->extra_headers
	);
	off = c->send.len;  // Start of body
	mg_printf(
			c,
			"<!DOCTYPE html><html><head><title>Index of %.*s</title>%s%s"
			"<link rel=\"stylesheet\" type=\"text/css\" href=\"/resources/bootstrap.css\"/>"
			"<style>body, html { margin: 0; padding: 0; }\n table { margin: 10px; }\n"
			"h1 { margin: 10px; }\n th,td {text-align: left; padding-right: 1em;"
			"font-size: 1rem; } </style></head>"
			"<body> <div class=\"navbar\"> <a href=\"/\">Go back</a> </div>"
			"<h1>Index of %.*s</h1> <table cellpadding=\"0\"> <thead>"
			"<tr> <th> <a href=\"#\" rel=\"0\">Name</a> </th>"
			"<th> <a href=\"#\" rel=\"1\">Modified</a> </th>"
			"<th> <a href=\"#\" rel=\"2\">Size</a> </th> </tr>"
			"<tr> <td colspan=\"3\"> <hr> </td> </tr>"
			"</thead>"
			"<tbody id=\"tb\">\n",
			(int)uri.len, uri.ptr, sort_js_code, sort_js_code2, (int)uri.len, uri.ptr
	);
	mg_printf(
			c, "%s",
			"  <tr><td><a href=\"..\">..</a></td>"
			"<td name=-1></td><td name=-1>[DIR]</td></tr>\n"
	);
	
	fs->ls(dir, printdirentry, &d);
	mg_printf(c, "</tbody><tfoot><tr><td colspan=\"3\"><hr></td></tr></tfoot> </table></body></html>\n");
	n = mg_snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)(c->send.len - off));
	if (n > sizeof(tmp)) n = 0;
	memcpy(c->send.buf + off - 12, tmp, n);  // Set content length
	c->is_resp = 0;                          // Mark response end
}

void serve_dir(struct mg_connection* c, struct mg_http_message* hm, const struct mg_http_serve_opts* opts)
{
	char path[MG_PATH_MAX];
	const char* sp = opts->ssi_pattern;
	int flags = uri_to_path(c, hm, opts, path, sizeof(path));
	if (flags < 0)
		return; // Do nothing: the response has already been sent by uri_to_path()
	
	if (flags & MG_FS_DIR)
	{
		list_dir(c, hm, opts, path);
		return;
	}
	
	if (flags && sp != nullptr && mg_globmatch(sp, strlen(sp), path, strlen(path)))
		mg_http_serve_ssi(c, opts->root_dir, path);
	else
		mg_http_serve_file(c, hm, path, opts);
}

inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Serving /dir/ to %s...", addr));
	char* path = nullptr;
	auto cwd = getcwd();
	
	std::string uri(msg->uri.ptr, msg->uri.len);
	strscanf(uri.c_str(), "/dir/%s", &path);
	
	std::string spath(secure_path(path ? path : ""));
	delete[] path;
	
	if (!spath.starts_with('/')) spath = "/" + spath;
	
	std::string sdpath(spath);
	sdpath.reserve(sdpath.size() + 1);
	mg_url_decode(sdpath.c_str(), sdpath.size(), sdpath.data(), sdpath.size() + 1, 0);
	sdpath = sdpath.data();
	
	struct stat st{ };
	if (::stat((cwd + sdpath).c_str(), &st) != 0)
	{
		send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
		return;
	}
	
	struct mg_http_serve_opts opts{ .root_dir = cwd.c_str() };
	std::string extra_header;
	
	if (st.st_size > MAX_INLINE_FILE_SIZE)
	{
		extra_header = "Content-Disposition: attachment; filename=\"";
		extra_header += path_basename(sdpath.c_str());
		extra_header += "\"\r\n";
		
		opts.extra_headers = extra_header.c_str();
	}
	
	std::string msgstrcp(msg->message.ptr, msg->uri.ptr);
	msgstrcp += spath;
	
	struct mg_http_message msg2{ };
	msg2.message = mg_str_n(msgstrcp.data(), msgstrcp.size());
	msg2.uri = mg_str_n(msg2.message.ptr + (msg->uri.ptr - msg->message.ptr), spath.size());
	msg2.method = msg->method;
	msg2.query = msg->query;
	msg2.proto = msg->proto;
	msg2.body = msg->body;
	msg2.head = msg->head;
	msg2.chunk = msg->chunk;
	for (size_t i = 0; i < MG_MAX_HTTP_HEADERS; ++i)
		msg2.headers[i] = msg->headers[i];
	
	serve_dir(connection, &msg2, &opts);
}


inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Serving /register-form to %s...", addr));
	mg_http_reply(connection, 200, "Content-Type: text/html\r\n", RESOURCE(register_html));
}


inline void send_confirmation_notification_page_html(struct mg_connection* connection)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Sending email confirmation page to %s...", addr));
	mg_http_reply(connection, 200, "Content-Type: text/html\r\n", RESOURCE(confirm_html));
}

typedef struct
{
	std::string message;
	size_t pos;
} MessageData;

// Callback function that provides the data for the email message
static size_t custom_curl_read_callback(void* buffer, size_t size, size_t nmemb, void* instream)
{
	auto* upload = (MessageData*)instream;
	
	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1) || upload->pos >= upload->message.size())
		return 0;
	
	size_t len = std::min(upload->message.size() - upload->pos, size * nmemb);
	
	memcpy(buffer, upload->message.c_str() + upload->pos, size * nmemb);
	upload->pos += len;
	
	return len;
}

inline id_t generate_id_and_send_email(struct mg_connection* connection, struct mg_http_message* msg, const std::string& email)
{
	auto server_address = mg_http_get_header(msg, "Host");
	
	if (server_address == nullptr)
		return 0;
	
	std::string server_address_str(server_address->ptr, server_address->len);
	
	srandom(mg_millis());
	
	id_t id = random();
	for (int i = 0; id == 0 || ftp_users_pending.contains(id) && i < 10; ++i) id = random();
	if (ftp_users_pending.contains(id))
		return 0;
	
	// Set up the curl handle
	CURL* curl = curl_easy_init();
	
	if (!curl)
	{
		MG_ERROR(("[Send Email] Unable to initialize curl."));
		return 0;
	}
	
	// Set the SMTP server and port
	curl_easy_setopt(curl, CURLOPT_URL, server_confirmator_smtp_server);
	
	// Set the username and password for authentication
	curl_easy_setopt(curl, CURLOPT_USERNAME, server_confirmator_email);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, server_confirmator_email_password);
	
	std::string link = "http" + std::string(connection->is_tls ? "s" : "") +
	                   "://" + server_address_str + "/confirm/" + std::to_string(id);
	
	// Define email message
	std::string message = "To: " + email + "\r\n" +
	                      "From: " + server_confirmator_email + "\r\n" +
	                      "Subject: Confirm registration of new account\r\n"
	                      "\r\n"
	                      "To complete registration open link " + link + " in any available browser.";
	
	MG_INFO(("Sending link [%s] to [%s]", link.c_str(), email.c_str()));
	
	struct curl_slist* recipients = nullptr;
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, server_confirmator_email);
	recipients = curl_slist_append(recipients, email.c_str());
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
	
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, custom_curl_read_callback);
	MessageData data{ .message = message, .pos = 0 };
	curl_easy_setopt(curl, CURLOPT_READDATA, &data);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	
	// Send the email
	CURLcode res = curl_easy_perform(curl);
	
	// Check for errors
	if (res != CURLE_OK)
		MG_ERROR(("[Send Email] curl_easy_perform() failed: %s\n", curl_easy_strerror(res)));
	
	// Clean up
	curl_slist_free_all(recipients);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	
	return id;
}

inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Processing registration request from %s...", addr));
	if (mg_strcmp(msg->method, mg_str("POST"))) return;
	
	char login[HOST_NAME_MAX]{ }, email[HOST_NAME_MAX]{ }, password[HOST_NAME_MAX]{ };
	mg_http_get_var(&msg->body, "login", login, sizeof(login));
	mg_http_get_var(&msg->body, "email", email, sizeof(email));
	mg_http_get_var(&msg->body, "password", password, sizeof(password));
	
	if (login[0] == 0)
	{
		mg_http_reply(connection, 400, "", "Login is required");
		return;
	}
	
	if (email[0] == 0)
	{
		mg_http_reply(connection, 400, "", "Email is required");
		return;
	}
	
	if (password[0] == 0 || strlen(password) < 8)
	{
		mg_http_reply(connection, 400, "", "Password is required and must be at least 8 characters long");
		return;
	}
	
	if (ftp_users.contains(login))
	{
		mg_http_reply(connection, 400, "", "User already exists");
		MG_INFO(("Blocked attempt to create existing user - '%s'.", login));
		return;
	}
	
	if (server_confirmator_email == nullptr)
	{
		auto user = ftp_users.insert({ login, { email, password } });
		if (!user.second)
		{
			mg_http_reply(connection, 400, "", "Cant insert user");
			return;
		}
		
		save_users();
		add_user(*user.first);
		mg_http_reply(connection, 200, "", "Success");
	}
	else
	{
		id_t id = generate_id_and_send_email(connection, msg, email);
		if (id == 0)
		{
			mg_http_reply(connection, 500, "", "Can't register new user");
			return;
		}
		
		ftp_users_pending[id] = { login, { email, password } };
		send_confirmation_notification_page_html(connection);
	}
}


inline void handle_confirm_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	if (server_confirmator_email == nullptr)
	{
		send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
		return;
	}
	
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Processing confirmation request from %s...", addr));
	if (mg_strcmp(msg->method, mg_str("GET"))) return;
	
	char* id_ptr = nullptr;
	
	std::string uri(msg->uri.ptr, msg->uri.len);
	strscanf(uri.c_str(), "/confirm/%s", &id_ptr);
	
	std::string id_str(secure_path(id_ptr ? id_ptr : ""));
	delete[] id_ptr;
	
	id_t id = 0;
	try
	{
		id = std::stoul(id_str);
	}
	catch (...)
	{
		mg_http_reply(connection, 400, "", "Invalid link");
		return;
	}
	
	if (id == 0)
	{
		mg_http_reply(connection, 400, "", "Invalid link");
		return;
	}
	
	auto pending_user = ftp_users_pending.find(id);
	if (pending_user == ftp_users_pending.end())
	{
		mg_http_reply(connection, 400, "", "Invalid link");
		return;
	}
	
	auto user = ftp_users.insert(pending_user->second);
	if (!user.second)
	{
		mg_http_reply(connection, 400, "", "Cant insert user");
		return;
	}
	
	save_users();
	add_user(*user.first);
	mg_http_reply(connection, 200, "", "Success");
}


inline void handle_resources_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char addr[20];
	mg_ntoa(&connection->rem, addr, sizeof addr);
	MG_INFO(("Processing /resources/ for %s...", addr));
	
	char* path = nullptr;
	
	std::string uri(msg->uri.ptr, msg->uri.len);
	strscanf(uri.c_str(), "/resources/%s", &path);
	
	std::string path_s(path);
	delete[] path;
	
	if (path_s == "bootstrap.css")
	{
		http_send_resource(connection, msg, RESOURCE(bootstrap_css), LEN(bootstrap_css), "text/css");
		return;
	}
	
	if (path_s == "CascadiaMono.woff")
	{
		http_send_resource(connection, msg, RESOURCE(CascadiaMono_woff), LEN(CascadiaMono_woff), "font/woff");
		return;
	}
	
	send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
}


int unlink_cb(const char* fpath, const struct stat*, int, struct FTW*)
{
	int rv = remove(fpath);
	if (rv) perror(fpath);
	return rv;
}

inline bool rm_rf(const char* path)
{
	struct stat st{ };
	if (stat(path, &st) < 0)
		return true;
	return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS) == 0;
}


inline bool mkdir_p(const char* path)
{
	struct stat st{ };
	if (stat(path, &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
			return true;
		rm_rf(path);
	}
	
	char tmp[256]{ };
	char* p = nullptr;
	size_t len;
	
	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for (p = tmp + 1; *p; p++)
		if (*p == '/')
		{
			*p = 0;
			mkdir(tmp, S_IRWXU);
			*p = '/';
		}
	return mkdir(tmp, S_IRWXU) == 0;
}


inline static void load_users()
{
	FILE* file = ::fopen("/etc/webserver.users", "rb");
	if (file)
	{
		while (!::feof(file))
		{
			char username[HOST_NAME_MAX + 1]{ };
			char email[HOST_NAME_MAX + 1]{ };
			char password[HOST_NAME_MAX + 1]{ };
			if (::fscanf(file, "%s : %s : %s\n", username, email, password) == 3) // Scan line in format "<user> : <password>\n"
				ftp_users[username] = { email, password }; // Store username and password
		}
		::fclose(file);
	}
}


inline static void save_users()
{
	FILE* file = ::fopen("/etc/webserver.users", "wb");
	if (!file)
		return;
	for (auto& ftp_user : ftp_users)
		::fprintf(
				file, "%s : %s : %s\n",
				ftp_user.first.c_str(), ftp_user.second.first.c_str(), ftp_user.second.second.c_str()
		);
	::fclose(file);
}


inline static void forward_all_users()
{
	std::string cwd(getcwd());
	cwd += '/';
	
	for (const auto& ftp_user : ftp_users)
	{
		std::string root_dir = cwd + ftp_user.first;
		if (mkdir_p(root_dir.c_str()))
		{
			MG_INFO(("[FTP] Adding user \"%s\" to ftp server...", ftp_user.first.c_str()));
			ftp_server.addUser(ftp_user.first, ftp_user.second.second, root_dir, fineftp::Permission::All);
		}
	}
}


inline static void add_user(decltype(*ftp_users.begin())& ftp_user)
{
	std::string cwd(getcwd());
	std::string root_dir = cwd + '/' + ftp_user.first;
	if (mkdir_p(root_dir.c_str()))
	{
		MG_INFO(("[FTP] Adding user \"%s\" to ftp server...", ftp_user.first.c_str()));
		ftp_server.addUser(ftp_user.first, ftp_user.second.second, root_dir, fineftp::Permission::All);
	}
}



inline static consteval size_t static_strlen(const char* str)
{
	size_t len = 0;
	for (; *str; ++len, ++str);
	return len;
}

inline bool starts_with(const char* str, const char* prefix)
{
	for (; *prefix; ++prefix, ++str)
		if (*prefix != *str)
			return false;
	return true;
}

inline std::string getcwd()
{
	char cwd[PATH_MAX]{ };
	getcwd(cwd, PATH_MAX);
	cwd[PATH_MAX - 1] = 0;
	std::string cwdstr(cwd);
	return std::move(cwdstr);
}


inline std::string erase_all(const std::string& str, const std::string& seq)
{
	std::string res(str, 0, seq.size());
	res.reserve(str.size()); // optional, avoids buffer reallocations in the loop
	for (size_t i = seq.size(); i < str.size(); ++i)
	{
		bool ok = false;
		for (int j = seq.size() - 1, k = i; j >= 0; --j, --k)
			if (seq[j] != str[k])
			{
				ok = true;
				break;
			}
		if (ok) res += str[i];
	}
	return std::move(res);
}

inline std::string secure_path(const std::string& path)
{
	std::string res = erase_all(path, "../");
	return erase_all(res, "/..");
}


inline std::string path_dirname(const std::string& path)
{
	if (path.empty()) return "";
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos) return "";
	return std::move(path.substr(0, slash));
}


inline std::string path_basename(std::string path)
{
	if (path.empty()) return "";
	while (path.ends_with('/')) path.pop_back();
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos) return "";
	return std::move(path.substr(slash + 1));
}


inline void http_send_resource(
		struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize, const char* mime_type
)
{
	int n, status = 200;
	char range[100]{ };
	int64_t r1 = 0, r2 = 0, cl = (int64_t)rcsize;
	struct mg_str mime = mg_str_s(mime_type);
	
	// Handle Range header
	struct mg_str* rh = mg_http_get_header(msg, "Range");
	range[0] = '\0';
	if (rh != nullptr && (n = getrange(rh, &r1, &r2)) > 0 && r1 >= 0 && r2 >= 0)
	{
		// If range is specified like "400-", set second limit to content len
		if (n == 1) r2 = cl - 1;
		if (r1 > r2 || r2 >= cl)
		{
			status = 416;
			cl = 0;
			mg_snprintf(range, sizeof(range), "Content-Range: bytes */%lld\r\n", (int64_t)rcsize);
		}
		else
		{
			status = 206;
			cl = r2 - r1 + 1;
			mg_snprintf(range, sizeof(range), "Content-Range: bytes %lld-%lld/%lld\r\n", r1, r1 + cl - 1, (int64_t)rcsize);
		}
	}
	mg_printf(
			connection,
			"HTTP/1.1 %d %s\r\n"
			"Content-Type: %.*s\r\n"
			"Content-Length: %llu\r\n"
			"%s\r\n",
			status, mg_http_status_code_str(status),
			(int)mime.len, mime.ptr,
			cl,
			range
	);
	
	if (mg_vcasecmp(&msg->method, "HEAD") == 0)
	{
		connection->is_draining = 1;
		connection->is_resp = 0;
		return;
	}
	
	
	typedef struct
	{
		const char* data;
		uint64_t len;
		uint64_t pos;
	} str_buf_fd;
	
	connection->pfn = [](struct mg_connection* c, int ev, void* ev_data, void* fn_data)
	{
		if (ev == MG_EV_WRITE || ev == MG_EV_POLL)
		{
			auto rc = (str_buf_fd*)fn_data;
			
			// Read to send IO buffer directly, avoid extra on-stack buffer
			size_t max = MG_IO_SIZE, space, * cl = (size_t*)c->label;
			if (c->send.size < max)
				mg_iobuf_resize(&c->send, max);
			if (c->send.len >= c->send.size)
				return;  // Rate limit
			if ((space = c->send.size - c->send.len) > *cl)
				space = *cl;
			
			memcpy(c->send.buf + c->send.len, &rc->data[rc->pos], space);
			rc->pos += space;
			c->send.len += space;
			*cl -= space;
			if (space == 0)
			{
				delete (str_buf_fd*)c->pfn_data;
				c->pfn_data = nullptr;
				c->pfn = http_cb;
				c->is_resp = 0;
			}
		}
		else if (ev == MG_EV_CLOSE)
		{
			delete (str_buf_fd*)c->pfn_data;
			c->pfn_data = nullptr;
			c->pfn = http_cb;
			c->is_resp = 0;
		}
	};
	connection->pfn_data = new str_buf_fd{ .data = rcdata, .len = rcsize, .pos = 0 };
	*(size_t*)connection->label = (size_t)cl;  // Track to-be-sent content length
}


inline void send_error_html(struct mg_connection* connection, int code, const char* color)
{
	mg_http_reply(
			connection, code, "Content-Type: text/html\r\n", RESOURCE(error_html),
			color, color, color, color, code, mg_http_status_code_str(code)
	);
}