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
#include "zip_dir.h"
#include "config.h"
#include <fineftp/server.h>
#include <map>
#include <ftw.h>


const char* http_address = DEFAULT_HTTP_SERVER_ADDRESS;
const char* https_address = DEFAULT_HTTPS_SERVER_ADDRESS;
const char* tls_path = nullptr;
int log_level = 2, hexdump = 0;

static struct mg_mgr manager{ };
static struct mg_connection* http_server_connection, * https_server_connection;
static fineftp::FtpServer ftp_server;
static std::map<std::string, std::string> ftp_users;

// Handle interrupts, like Ctrl-C
static int s_signo = 0;

static void signal_handler(int signo)
{
	MG_ERROR(("[SIGNAL_HANDLER] Captured SIG%s(%d) \"%s\".", sigabbrev_np(signo), signo, sigdescr_np(signo)));
	s_signo = signo;
}


typedef struct
{
	const char* data;
	uint64_t len;
	uint64_t pos;
} str_buf_fd;


typedef struct
{
	std::string path;
	std::string description;
	path_handler_function fn;
} registered_path_handler;

typedef struct __registered_path_handlers
{
	registered_path_handler* data = nullptr;
	struct __registered_path_handlers* next = nullptr;
} registered_path_handlers;

static registered_path_handlers* handlers = nullptr;
static registered_path_handlers* handlers_head = nullptr;


/// Return true if str starts with prefix
inline bool starts_with(const char* str, const char* prefix);

/// Get current working directory
inline char* getcwd();

/// Erase all seq occurrences in str
inline std::string erase_all(const std::string& str, const std::string& seq);

/// Remove .. subdirectories from path to prevent sandbox escape
inline std::string secure_path(const std::string& path);

/// Get parent directory name of entry at given path
inline char* path_dirname(const char* path);

/// Get entry base name
inline const char* path_basename(const char* path);

/// rm -rf
inline bool rm_rf(const char* path);

/// mkdir -p
inline bool mkdir_p(const char* path);

/// Load ftp users from file
inline static void load_users();

/// Save ftp users into file
inline static void save_users();

/// Create users on ftp server
inline static void refresh_users();

/// Create user on ftp server
inline static void add_user(decltype(*ftp_users.begin())& ftp_user);


/// Send resource string as regular file over http
inline void http_send_resource_file(struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize);

/// Send error page over http
inline void send_error_html(struct mg_connection* connection, int code, const char* color);


/// Handle index page access
inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle favicon access
inline void handle_favicon_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle filesystem access (serve directory)
inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle filesystem access (serve directory)
inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle filesystem access (serve directory)
inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg);



/// Add path handler to global linked list
void register_path_handler(const std::string& path, const std::string& description, path_handler_function fn)
{
	if (!handlers_head)
	{
		delete handlers;
		handlers = new registered_path_handlers{
				.data = new registered_path_handler{ .path = path, .description = description, .fn = fn },
				.next = nullptr };
		handlers_head = handlers;
	}
	else
	{
		handlers_head->next = new registered_path_handlers{
				.data = new registered_path_handler{ .path = path, .description = description, .fn = fn },
				.next = nullptr };
		handlers_head = handlers_head->next;
	}
}

/// Iterate through registered handlers and try handle them
inline void handle_registered_paths(struct mg_connection* connection, struct mg_http_message* msg)
{
	registered_path_handlers* root = handlers;
	
	while (root && root->data && !starts_with(msg->uri.ptr, root->data->path.c_str()))
		root = root->next;
	
	if (root && root->data)
		return root->data->fn(connection, msg);
	
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
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGKILL, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGHUP, signal_handler);
	
	mg_log_set(log_level);
	mg_mgr_init(&manager);
	
	register_additional_handlers();
	
	
	ftp_server.addUserAnonymous(getcwd(), fineftp::Permission::ReadOnly);
	
	load_users();
	refresh_users();
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
		if (!(https_server_connection = mg_http_listen(&manager, https_address, client_handler, (void*)1)))
		{
			MG_ERROR(("Cannot start listening on %s. Use 'https://ADDR:PORT' or just ':PORT' as https address parameter", https_address));
			exit(EXIT_FAILURE);
		}
	
	if (hexdump) http_server_connection->is_hexdumping = 1;
	if (hexdump) https_server_connection->is_hexdumping = 1;
	
	auto cwd = getcwd();
	
	MG_INFO((""));
	MG_INFO(("Mongoose v" MG_VERSION));
	MG_INFO(("Server listening on : [%s]", http_address));
	MG_INFO(("Web root directory  : [file://%s/]", cwd));
	MG_INFO((""));
	
	if (tls_path)
	{
		MG_INFO((""));
		MG_INFO(("Mongoose v" MG_VERSION));
		MG_INFO(("Server listening on : [%s]", https_address));
		MG_INFO(("Web root directory  : [file://%s/]", cwd));
		MG_INFO((""));
	}
	
	ftp_server.start(4);
	
	delete[] cwd;
	
	while (s_signo == 0) mg_mgr_poll(&manager, 1000);
	
	mg_mgr_free(&manager);
	ftp_server.stop();
	MG_INFO(("Exiting due to signal [%d]...", s_signo));
}



inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	size_t accumulate = 0, count = 0;
	std::string appendix;
	for (auto* i = handlers; i != nullptr && i->data != nullptr;
	     accumulate += i->data->path.size() + i->data->description.size(), ++count, i = i->next)
	{
		appendix += "<li><a href=\"" + i->data->path + "\">" + i->data->description + "</a></li>\n";
	}
	
	char article_complete[article_html_len + count * 20 + accumulate + 1];
	sprintf(article_complete, reinterpret_cast<const char*>(article_html), appendix.c_str());
	
	mg_http_reply(
			connection, 200, "Content-Type: text/html\r\n",
			reinterpret_cast<const char*>(index_html), article_complete
	);
}


inline void handle_favicon_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	http_send_resource_file(connection, msg, reinterpret_cast<const char*>(favicon_ico), favicon_ico_len);
}


inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	char* path = nullptr;
	
	char* uri = new char[msg->uri.len + 1];
	strncpy(uri, msg->uri.ptr, msg->uri.len);
	uri[msg->uri.len] = 0;
	
	::strscanf(uri, "/dir/%s", &path);
	
	std::string strpath(path ? path : "");
	
	delete[] uri;
	delete[] path;
	
	std::string spath(secure_path(strpath));
	
	if (!spath.starts_with('/')) spath = "/" + spath;
	std::string spath_full(getcwd() + spath);
	
	struct stat st{ };
	if (::stat(spath_full.c_str(), &st) == 0)
	{
		struct mg_http_serve_opts opts{ .root_dir = getcwd() };
		std::string extra_header;
		
		if (st.st_size > MAX_INLINE_FILE_SIZE)
		{
			extra_header = "Content-Disposition: attachment; filename=\"";
			extra_header += path_basename(spath.c_str());
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
		
		mg_http_serve_dir(connection, &msg2, &opts);
	}
	else send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
}


inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	mg_http_reply(connection, 200, "Content-Type: text/html\r\n", reinterpret_cast<const char*>(register_html));
}


inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	if (!mg_strcmp(msg->method, mg_str("POST")))
	{
		char login[HOST_NAME_MAX], password[HOST_NAME_MAX];
		mg_http_get_var(&msg->body, "login", login, sizeof(login));
		mg_http_get_var(&msg->body, "password", password, sizeof(password));
		if (login[0] == 0)
			mg_http_reply(connection, 400, "", "Login is required");
		else if (password[0] == 0)
			mg_http_reply(connection, 400, "", "Password is required and must be at least 8 characters long");
		else
		{
			ftp_users[login] = password;
			save_users();
			add_user(*ftp_users.find(login));
			mg_http_reply(connection, 200, "", "Success");
		}
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

inline char* getcwd()
{
	char* cwd = new char[PATH_MAX];
	getcwd(cwd, PATH_MAX);
	cwd[PATH_MAX - 1] = 0;
	return cwd;
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

inline char* path_dirname(const char* path)
{
	if (!path) return new char[1]{ };
	char* accesible_path = ::strdup(path);
	char* slash = nullptr;
	for (char* tmp = accesible_path; *tmp; ++tmp)
		if (*tmp == '/')
			slash = tmp;
	
	if (!slash) return new char[1]{ };
	*slash = 0;
	return accesible_path;
}

inline const char* path_basename(const char* path)
{
	if (!path) return "";
	const char* slash = nullptr;
	for (const char* tmp = path; *tmp; ++tmp)
		if (*tmp == '/')
			slash = tmp;
	
	if (!slash) return "";
	return slash + 1;
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
		if (S_ISDIR(st.st_mode)) return true;
		else rm_rf(path);
	}
	
	char tmp[256];
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
			char password[200]{ };
			if (::fscanf(file, "%s : %s\n", username, password) == 2) // Scan line in format "<user> : <password>\n"
				ftp_users[username] = password; // Store username and password
		}
		::fclose(file);
	}
}


inline static void save_users()
{
	FILE* file = ::fopen("/etc/webserver.users", "wb");
	if (file)
	{
		for (auto& ftp_user : ftp_users)
			::fprintf(file, "%s : %s\n", ftp_user.first.c_str(), ftp_user.second.c_str());
		::fclose(file);
	}
}


inline static void refresh_users()
{
	std::string cwd(getcwd());
	cwd += '/';
	
	for (const auto& ftp_user : ftp_users)
	{
		std::string root_dir = cwd + ftp_user.first;
		mkdir_p(root_dir.c_str());
		ftp_server.addUser(ftp_user.first, ftp_user.second, root_dir, fineftp::Permission::All);
	}
}


inline static void add_user(decltype(*ftp_users.begin())& ftp_user)
{
	std::string cwd(getcwd());
	std::string root_dir = cwd + '/' + ftp_user.first;
	mkdir_p(root_dir.c_str());
	ftp_server.addUser(ftp_user.first, ftp_user.second, root_dir, fineftp::Permission::All);
}


static void restore_http_cb_rp(struct mg_connection* c)
{
	delete (str_buf_fd*)c->pfn_data;
	c->pfn_data = nullptr;
	c->pfn = http_cb;
	c->is_resp = 0;
}

static void static_resource_send(struct mg_connection* c, int ev, void* ev_data, void* fn_data)
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
		if (space == 0) restore_http_cb_rp(c);
	}
	else if (ev == MG_EV_CLOSE)
	{
		restore_http_cb_rp(c);
	}
}

inline void http_send_resource_file(struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize)
{
	char etag[64], tmp[MG_PATH_MAX];
	time_t mtime = 0;
	struct mg_str* inm = nullptr;
	
	if (mg_http_etag(etag, sizeof(etag), rcsize, mtime) != nullptr &&
	    (inm = mg_http_get_header(msg, "If-None-Match")) != nullptr &&
	    mg_vcasecmp(inm, etag) == 0)
	{
		mg_http_reply(connection, 304, nullptr, "");
	}
	else
	{
		int n, status = 200;
		char range[100];
		int64_t r1 = 0, r2 = 0, cl = (int64_t)rcsize;
		struct mg_str mime = MG_C_STR("image/x-icon");
		
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
				"Etag: %s\r\n"
				"Content-Length: %llu\r\n"
				"%s\r\n",
				status, mg_http_status_code_str(status),
				(int)mime.len, mime.ptr,
				etag,
				cl,
				range
		);
		if (mg_vcasecmp(&msg->method, "HEAD") == 0)
		{
			connection->is_draining = 1;
			connection->is_resp = 0;
		}
		else
		{
			connection->pfn = static_resource_send;
			connection->pfn_data = new str_buf_fd{ .data = rcdata, .len = rcsize, .pos = 0 };
			*(size_t*)connection->label = (size_t)cl;  // Track to-be-sent content length
		}
	}
}

inline void send_error_html(struct mg_connection* connection, int code, const char* color)
{
	mg_http_reply(
			connection, code, "Content-Type: text/html\r\n", reinterpret_cast<const char*>(error_html),
			color, color, color, color, code, mg_http_status_code_str(code)
	);
}