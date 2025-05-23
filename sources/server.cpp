// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.


#include "server.h"
#include "../mongoose.c"
#include "constants.hpp"
#include "../strscan.c"
#include "../resources.hpp"
#include "tools.h"
#include "config.h"


#ifdef ENABLE_FILESYSTEM_ACCESS

#include "../ftp/ftp_event_handler.h"
#include <fineftp/server.h>


#endif


#include <map>
#include <ftw.h>
#include <curl/curl.h>


typedef id_t uint32_t;

const char* http_address = DEFAULT_HTTP_SERVER_ADDRESS;
const char* https_address = DEFAULT_HTTPS_SERVER_ADDRESS;
const char* tls_path = nullptr;
const char* server_verification_email = nullptr;
const char* server_verification_email_password = nullptr;
const char* server_verification_smtp_server = "smtps://smtp.gmail.com:465";
int log_level = 2, hexdump = 0;

#ifdef ENABLE_FILESYSTEM_ACCESS
pthread_mutex_t ftp_callback_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static struct mg_mgr manager { };
static struct mg_connection* http_server_connection = nullptr, * https_server_connection = nullptr;

#ifdef ENABLE_FILESYSTEM_ACCESS
static fineftp::FtpServer ftp_server;
#endif

static std::map<std::string, std::pair<std::string, std::string>> registered_users;
static std::map<id_t, std::pair<std::string, std::pair<std::string, std::string>>> registered_users_pending;

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


/// rm -rf
inline bool rm_rf(const std::string& path);

/// mkdir -p
inline bool mkdir_p(const std::string& path);

/// Load registered users from file
inline static void load_users();

/// Save registered users to file
inline static void save_users();

#ifdef ENABLE_FILESYSTEM_ACCESS

/// Create all registered users on ftp server
inline static void forward_all_users();

/// Create registered user on ftp server
inline static void add_user(decltype(*registered_users.begin())& reg_user);

#endif


/// Send icon resource string as regular file over http
inline void http_send_resource(
		struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize, const char* mime_type
);

/// Send error page over http
inline void send_error_html(struct mg_connection* connection, int code, const char* color, const char* msg);


/// Handle index page access
inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle favicon access
inline void handle_favicon_ico(struct mg_connection* connection, struct mg_http_message* msg);

#ifdef ENABLE_FILESYSTEM_ACCESS

/// Handle filesystem access (serve directory)
inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg);

#endif

/// Handle user registration form access
inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user registration request
inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user email verification request
inline void handle_verify_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle other resources request
inline void handle_resources_html(struct mg_connection* connection, struct mg_http_message* msg);


extern const auto* get_registered_users()
{
	return &registered_users;
}

/// Add path handler to global linked list
void register_path_handler(
		const std::string& path, const std::string& description, path_handler_function fn,
		decltype(registered_path_handler::restriction_type) type
)
{
	MG_INFO(("Registering '%s' => '%s' path...", description.c_str(), path.c_str()));
	if (!handlers_head)
	{
		handlers = new registered_path_handlers {
				.data = new registered_path_handler {.path = path, .description = description, .fn = fn, .restriction_type = type},
				.next = nullptr
		};
		handlers_head = handlers;
	}
	else
	{
		handlers_head->next = new registered_path_handlers {
				.data = new registered_path_handler {.path = path, .description = description, .fn = fn, .restriction_type = type},
				.next = nullptr
		};
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
		MG_INFO(("Handling '%s' => '%s' path for IP %M...", root->data->description.c_str(), root->data->path.c_str(),
				mg_print_ip, &connection->rem));
		return root->data->fn(connection, msg);
	}

	send_error_html(connection, COLORED_ERROR(404), "");
}


inline void handle_http_message(struct mg_connection* connection, struct mg_http_message* msg)
{
	if (mg_http_match_uri(msg, "/") || mg_http_match_uri(msg, "/index.html") || mg_http_match_uri(msg, "/index"))
		handle_index_html(connection, msg);
	else if (mg_http_match_uri(msg, "/favicon.ico") || mg_http_match_uri(msg, "/favicon"))
		handle_favicon_ico(connection, msg);
#ifdef ENABLE_FILESYSTEM_ACCESS
	else if (starts_with(msg->uri.ptr, "/dir/"))
		handle_dir_html(connection, msg);
#endif
	else if (mg_http_match_uri(msg, "/register-form"))
		handle_register_form_html(connection, msg);
	else if (mg_http_match_uri(msg, "/register"))
		handle_register_html(connection, msg);
	else if (starts_with(msg->uri.ptr, "/verify/"))
		handle_verify_html(connection, msg);
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
				.key = key_path.c_str(), // This pem contains both cert and key
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
	if (server_verification_email != nullptr && server_verification_email_password == nullptr)
	{
		puts("[Server] An error occurred during server initialization: No password was given for verification email.");
		exit(-3);
	}

	if (server_verification_email == nullptr || *server_verification_email == 0)
	{
		puts("[Server] ======= HEADS UP ======");
		puts("[Server] Verification email was not configured. Entering unsafe mode!!!");
		puts("[Server] !!! Heads up! Your server is vulnerable to spam attacks! !!!");
		puts("[Server] Please, consider creating a google account and set it up as verification email.");
		puts("[Server] ======= HEADS UP ======");
		delete[] server_verification_email;
		delete[] server_verification_email_password;
		server_verification_email = server_verification_email_password = nullptr;
	}

	// Initialize the libcurl library
	curl_global_init(CURL_GLOBAL_ALL);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGQUIT, signal_handler);

	mg_log_set(log_level);
	mg_mgr_init(&manager);

#ifdef ENABLE_FILESYSTEM_ACCESS
	if (log_level >= 2)
	{
		add_custom_ftp_handler(
				[](
						const std::string& ftp_command, const std::string& parameters, const std::string& ftp_working_directory,
						std::shared_ptr<::fineftp::FtpUser> ftp_user
				) {
					mutex_locker l(&ftp_callback_mutex);
					MG_INFO((" [FTP] %s %s", ftp_command.c_str(), parameters.c_str()));
				}
		);
	}
#endif

#ifdef ENABLE_FILESYSTEM_ACCESS
	register_additional_handlers();

	MG_INFO(("[FTP] Adding anonymous user to ftp server..."));
	ftp_server.addUserAnonymous(getcwd(), fineftp::Permission::ReadOnly);
#endif

	MG_INFO(("[USERS] Loading users from file..."));
	load_users();

#ifdef ENABLE_FILESYSTEM_ACCESS
	MG_INFO(("[FTP] Forwarding users to ftp server..."));
	forward_all_users();
#endif
}

/// Start listening on given http_address and run server loop
void server_run()
{
	if (!(http_server_connection = mg_http_listen(&manager, http_address, client_handler, nullptr)))
	{
		MG_ERROR(
				("Could not start listening on %s. Use 'http://ADDR:PORT' or just ':PORT' as http address parameter", http_address));
		exit(EXIT_FAILURE);
	}

	if (tls_path)
	{
		if (!(https_server_connection = mg_http_listen(&manager, https_address, client_handler, (void*)1)))
		{
			MG_ERROR(
					("Could not start listening on %s. Use 'https://ADDR:PORT' or just ':PORT' as https address parameter", https_address));
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
	MG_INFO(("Server is listening on : [%s]", http_address));
	MG_INFO(("Web root directory  : [file://%s/]", cwd.c_str()));
	MG_INFO((""));

	if (https_server_connection)
	{
		MG_INFO((""));
		MG_INFO(("Mongoose v" MG_VERSION));
		MG_INFO(("Server is listening on : [%s]", https_address));
		MG_INFO(("Web root directory  : [file://%s/]", cwd.c_str()));
		MG_INFO((""));
	}

#ifdef ENABLE_FILESYSTEM_ACCESS
	ftp_server.start(4);
	MG_INFO(("[FTP]"));
	MG_INFO(("[FTP] Started ftp server on : [ftp://%s:%d]", ftp_server.getAddress().c_str(), ftp_server.getPort()));
	MG_INFO(("[FTP] Web root directory    : [file://%s/]", cwd.c_str()));
	MG_INFO(("[FTP]"));
#endif

	while (s_signo == 0) mg_mgr_poll(&manager, 1000);

	mg_mgr_free(&manager);
#ifdef ENABLE_FILESYSTEM_ACCESS
	ftp_server.stop();
#endif
	MG_INFO(("Exiting due to signal [%d]...", s_signo));

	exit(s_signo);
}


inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	MG_INFO(("Serving index.html to %M...", mg_print_ip, &connection->rem));
	std::string list_html;
#ifdef ENABLE_FILESYSTEM_ACCESS
	list_html += "<li><a href=\"/dir/\">Observe directory structure</a></li>\n";
#endif
	list_html += "<li><a href=\"/register-form\">Create a new account</a></li>\n";
	for (auto* i = handlers; i != nullptr && i->data != nullptr; i = i->next)
	{
		list_html += "<li><a href=\"" + i->data->path + "\">" + i->data->description + "</a></li>\n";
		MG_INFO(("Indexed '%s' => '%s'.", i->data->description.c_str(), i->data->path.c_str()));
	}

	char article_complete[LEN(article_html) + list_html.size() + 1];
	sprintf(article_complete, RESOURCE(article_html), list_html.c_str());

	mg_http_reply(
			connection, 200, "Content-Type: text/html\r\n",
			RESOURCE(index_html), article_complete
	);
}


inline void handle_favicon_ico(struct mg_connection* connection, struct mg_http_message* msg)
{
	MG_INFO(("Serving favicon.ico to %M...", mg_print_ip, &connection->rem));
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
	struct printdirentrydata d = {c, hm, opts, dir};
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

#ifdef ENABLE_FILESYSTEM_ACCESS

inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	MG_INFO(("Serving /dir/ to %M...", mg_print_ip, &connection->rem));
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

	struct stat st { };
	if (::stat((cwd + sdpath).c_str(), &st) != 0)
	{
		send_error_html(connection, COLORED_ERROR(404), "");
		return;
	}

	struct mg_http_serve_opts opts {.root_dir = cwd.c_str()};
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

	struct mg_http_message msg2 { };
	msg2.message = mg_str_n(msgstrcp.data(), msgstrcp.size());
	msg2.uri = mg_str_n(msg2.message.ptr + (msg->uri.ptr - msg->message.ptr), spath.size());
	msg2.method = msg->method;
	msg2.query = msg->query;
	msg2.proto = msg->proto;
	msg2.body = msg->body;
	msg2.head = msg->head;
	for (size_t i = 0; i < MG_MAX_HTTP_HEADERS; ++i)
		msg2.headers[i] = msg->headers[i];

	serve_dir(connection, &msg2, &opts);
}

#endif


inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	MG_INFO(("Serving /register-form to %M...", mg_print_ip, &connection->rem));
	mg_http_reply(connection, 200, "Content-Type: text/html\r\n", RESOURCE(register_html));
}


inline void send_verification_notification_page_html(struct mg_connection* connection)
{
	MG_INFO(("Sending email verification page to %M...", mg_print_ip, &connection->rem));
	mg_http_reply(connection, 200, "Content-Type: text/html\r\n", RESOURCE(verify_html));
}

typedef struct
{
	std::string message;
	size_t pos;
} MessageData;

// Callback function that provides the data for the email message
static size_t curl_read_callback_email_data(void* buffer, size_t size, size_t nmemb, void* instream)
{
	auto* upload = (MessageData*)instream;

	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1) || upload->pos >= upload->message.size())
		return 0;

	size_t len = std::min(upload->message.size() - upload->pos, size * nmemb);

	memcpy(buffer, upload->message.c_str() + upload->pos, len);
	upload->pos += len;

	return len;
}

inline id_t generate_id_and_send_email(struct mg_connection* connection, struct mg_http_message* msg, const std::string& email)
{
	auto email_hostaddr_pos = email.find('@');
	if (email_hostaddr_pos == std::string::npos)
		return 0;

	if (!server_verification_email_hosts_whitelist.empty() &&
	    !server_verification_email_hosts_whitelist.contains(email.substr(email_hostaddr_pos + 1)))
	{
		send_error_html(connection, COLORED_ERROR(406), "This email service provider is not allowed");
		return 0;
	}

	if (!server_verification_email_hosts_blacklist.empty() &&
	    server_verification_email_hosts_blacklist.contains(email.substr(email_hostaddr_pos + 1)))
	{
		send_error_html(connection, COLORED_ERROR(406), "This email service provider is not allowed");
		return 0;
	}

	auto server_address = mg_http_get_header(msg, "Host");

	if (server_address == nullptr)
	{
		send_error_html(connection, COLORED_ERROR(500), "Can't generate a verification link");
		return 0;
	}

	std::string server_address_str(server_address->ptr, server_address->len);

	srandom(mg_millis());

	id_t id = random();
	for (int i = 0; id == 0 || registered_users_pending.contains(id) && i < 10; ++i) id = random();
	if (registered_users_pending.contains(id))
		return 0;

	// Set up the curl handle
	CURL* curl = curl_easy_init();

	if (!curl)
	{
		MG_ERROR(("[Send Email] Unable to initialize curl."));
		send_error_html(connection, COLORED_ERROR(500), "Can't generate a verification link");
		return 0;
	}

	// Set the SMTP server and port
	curl_easy_setopt(curl, CURLOPT_URL, server_verification_smtp_server);

	// Set the username and password for authentication
	curl_easy_setopt(curl, CURLOPT_USERNAME, server_verification_email);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, server_verification_email_password);

	std::string link = "http" + std::string(connection->is_tls ? "s" : "") +
	                   "://" + server_address_str + "/verify/" + std::to_string(id);

	// Define email message
	std::string message = "To: " + email + "\r\n" +
	                      "From: " + server_verification_email + "\r\n" +
	                      "Subject: Confirm registration of new account\r\n"
	                      "\r\n"
	                      "To verify your email account and complete the registration process open link " + link +
	                      " in any available web browser.";

	MG_INFO(("Sending link [%s] to [%s]", link.c_str(), email.c_str()));

	struct curl_slist* recipients = nullptr;
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, server_verification_email);
	recipients = curl_slist_append(recipients, email.c_str());
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_callback_email_data);
	MessageData data {.message = message, .pos = 0};
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
	MG_INFO(("Processing registration request from %M...", mg_print_ip, &connection->rem));
	if (mg_strcmp(msg->method, mg_str("POST"))) return;

	char login[HOST_NAME_MAX] { }, email[HOST_NAME_MAX] { }, password[HOST_NAME_MAX] { };
	mg_http_get_var(&msg->body, "login", login, sizeof(login));
	mg_http_get_var(&msg->body, "email", email, sizeof(email));
	mg_http_get_var(&msg->body, "password", password, sizeof(password));

	if (login[0] == 0)
	{
		send_error_html(connection, COLORED_ERROR(406), "Login is required");
		return;
	}

	if (email[0] == 0)
	{
		send_error_html(connection, COLORED_ERROR(406), "Email is required");
		return;
	}

	if (password[0] == 0 || strlen(password) < 8)
	{
		send_error_html(connection, COLORED_ERROR(406), "Password is required and must be at least 8 characters long");
		return;
	}

	if (registered_users.contains(login))
	{
		send_error_html(connection, COLORED_ERROR(409), "User already exists");
		MG_INFO(("Blocked attempt to create existing user - '%s'.", login));
		return;
	}

	if (server_verification_email == nullptr)
	{
		auto user = registered_users.insert({login, {email, password}});
		if (!user.second)
		{
			send_error_html(connection, COLORED_ERROR(500), "Could not insert user");
			return;
		}

		save_users();
#ifdef ENABLE_FILESYSTEM_ACCESS
		add_user(*user.first);
#endif
		mg_http_reply(connection, 200, "", "Success");
	}
	else
	{
		id_t id = generate_id_and_send_email(connection, msg, email);
		if (id == 0) return;

		registered_users_pending[id] = {login, {email, password}};
		send_verification_notification_page_html(connection);
	}
}


inline void handle_verify_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	if (server_verification_email == nullptr)
	{
		send_error_html(connection, COLORED_ERROR(405), "Email authorization in disabled");
		return;
	}

	MG_INFO(("Processing verification request from %M...", mg_print_ip, &connection->rem));
	if (mg_strcmp(msg->method, mg_str("GET"))) return;

	char* id_ptr = nullptr;

	std::string uri(msg->uri.ptr, msg->uri.len);
	strscanf(uri.c_str(), "/verify/%s", &id_ptr);

	std::string id_str(secure_path(id_ptr ? id_ptr : ""));
	delete[] id_ptr;

	id_t id = 0;
	try
	{
		id = std::stoul(id_str);
	}
	catch (...)
	{
		send_error_html(connection, COLORED_ERROR(403), "Invalid link");
		return;
	}

	if (id == 0)
	{
		send_error_html(connection, COLORED_ERROR(403), "Invalid link");
		return;
	}

	auto pending_user = registered_users_pending.find(id);
	if (pending_user == registered_users_pending.end())
	{
		send_error_html(connection, COLORED_ERROR(403), "Invalid link");
		return;
	}

	auto user = registered_users.insert(pending_user->second);
	if (!user.second)
	{
		send_error_html(connection, COLORED_ERROR(500), "Could not insert user");
		return;
	}

	save_users();
#ifdef ENABLE_FILESYSTEM_ACCESS
	add_user(*user.first);
#endif
	mg_http_reply(connection, 200, "", "Success");
}


inline void handle_resources_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	MG_INFO(("Processing /resources/ for %M...", mg_print_ip, &connection->rem));

	char* path = nullptr;

	std::string uri(msg->uri.ptr, msg->uri.len);
	strscanf(uri.c_str(), "/resources/%s", &path);

	std::string path_s(path);
	delete[] path;

	if (path_s == "bootstrap.css")
		http_send_resource(connection, msg, RESOURCE(bootstrap_css), LEN(bootstrap_css), "text/css");
	else if (path_s == "CascadiaMono.woff")
		http_send_resource(connection, msg, RESOURCE(CascadiaMono_woff), LEN(CascadiaMono_woff), "font/woff");
	else
		send_error_html(connection, COLORED_ERROR(404), "This resource does not exist");
}


int unlink_cb(const char* fpath, const struct stat*, int, struct FTW*)
{
	int rv = remove(fpath);
	if (rv) perror(fpath);
	return rv;
}

inline bool rm_rf(const char* path)
{
	struct stat st { };
	if (stat(path, &st) < 0)
		return true;
	return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS) == 0;
}


inline bool mkdir_p(const char* path)
{
	struct stat st { };
	if (stat(path, &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
			return true;
		rm_rf(path);
	}

	char tmp[256] { };
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
			char username[HOST_NAME_MAX + 1] { };
			char email[HOST_NAME_MAX + 1] { };
			char password[HOST_NAME_MAX + 1] { };
			if (::fscanf(file, "%s : %s : %s\n", username, email, password) == 3) // Scan line in format "<user> : <password>\n"
				registered_users[username] = {email, password}; // Store username and password
		}
		::fclose(file);
	}
}


inline static void save_users()
{
	FILE* file = ::fopen("/etc/webserver.users", "wb");
	if (!file)
		return;
	for (auto& reg_user : registered_users)
		::fprintf(
				file, "%s : %s : %s\n",
				reg_user.first.c_str(), reg_user.second.first.c_str(), reg_user.second.second.c_str()
		);
	::fclose(file);
}


#ifdef ENABLE_FILESYSTEM_ACCESS

inline static void forward_all_users()
{
	std::string cwd(getcwd());
	cwd += '/';

	for (const auto& reg_user : registered_users)
	{
		std::string root_dir = cwd + reg_user.first;
		if (mkdir_p(root_dir.c_str()))
		{
			MG_INFO(("[FTP] Adding user \"%s\" to ftp server...", reg_user.first.c_str()));
			ftp_server.addUser(reg_user.first, reg_user.second.second, root_dir, fineftp::Permission::All);
		}
	}
}


inline static void add_user(decltype(*registered_users.begin())& reg_user)
{
	std::string cwd(getcwd());
	std::string root_dir = cwd + '/' + reg_user.first;
	if (mkdir_p(root_dir.c_str()))
	{
		MG_INFO(("[FTP] Adding user \"%s\" to ftp server...", reg_user.first.c_str()));
		ftp_server.addUser(reg_user.first, reg_user.second.second, root_dir, fineftp::Permission::All);
	}
}

#endif


inline void http_send_resource(
		struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize, const char* mime_type
)
{
	int n, status = 200;
	char range[100] { };
	size_t r1 = 0, r2 = 0, cl = rcsize;
	struct mg_str mime = mg_str_s(mime_type);

	// Handle Range header
	struct mg_str* rh = mg_http_get_header(msg, "Range");
	range[0] = '\0';
	if (rh != nullptr && (n = getrange(rh, &r1, &r2)) > 0)
	{
		// If range is specified like "400-", set second limit to content len
		if (n == 1) r2 = cl - 1;
		if (r1 > r2 || r2 >= cl)
		{
			status = 416;
			cl = 0;
			mg_snprintf(range, sizeof(range), "Content-Range: bytes */%llu\r\n", rcsize);
		}
		else
		{
			status = 206;
			cl = r2 - r1 + 1;
			mg_snprintf(range, sizeof(range), "Content-Range: bytes %llu%llu/%llu\r\n", r1, r1 + cl - 1, rcsize);
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

	connection->pfn = [](struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
		if (ev == MG_EV_WRITE || ev == MG_EV_POLL)
		{
			auto rc = (str_buf_fd*)fn_data;

			// Read to send IO buffer directly, avoid extra on-stack buffer
			size_t max = MG_IO_SIZE, space;
			size_t* cl = (size_t*)&c->data[(sizeof(c->data) - sizeof(size_t)) /
			                               sizeof(size_t) * sizeof(size_t)];
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
	// Track to-be-sent content length at the end of connection->data, aligned
	size_t* clp = (size_t*)&connection->data[(sizeof(connection->data) - sizeof(size_t)) /
	                                         sizeof(size_t) * sizeof(size_t)];
	connection->pfn_data = new str_buf_fd {.data = rcdata, .len = rcsize, .pos = 0};
	*clp = (size_t)cl;  // Track to-be-sent content length
}


inline void send_error_html(struct mg_connection* connection, int code, const char* color, const char* msg)
{
	mg_http_reply(
			connection, code, "Content-Type: text/html\r\n", RESOURCE(error_html),
			color, color, color, color, code, mg_http_status_code_str(code), msg
	);
}