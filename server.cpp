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


const char* address = DEFAULT_SERVER_ADDRESS;
int log_level = 2, hexdump = 0;

static struct mg_mgr manager{ };
static struct mg_connection* server_connection;

// Handle interrupts, like Ctrl-C
static int s_signo = 0;

static void signal_handler(int signo)
{
	MG_ERROR(("[SIGNAL] Received SIG%s \"%s\". Closing server...", sigabbrev_np(signo), sigdescr_np(signo)));
	s_signo = signo;
}


typedef struct
{
	const char* data;
	uint64_t len;
	uint64_t pos;
} str_buf_fd;



inline int starts_with(const char* str, const char* prefix);

inline char* getcwd();

inline std::string erase_all(const std::string& str, const std::string& seq);

inline std::string secure_path(const std::string& path);

inline char* path_dirname(const char* path);

inline const char* path_basename(const char* path);


inline void http_send_resource_file(struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize);

inline void send_error_html(struct mg_connection* connection, int code, const char* color);


inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg);

inline void handle_favicon_html(struct mg_connection* connection, struct mg_http_message* msg);

inline void handle_file_html(struct mg_connection* connection, struct mg_http_message* msg);

inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg);



inline void handle_http_message(struct mg_connection* connection, struct mg_http_message* msg)
{
	if (mg_http_match_uri(msg, "/") || mg_http_match_uri(msg, "/index.html") || mg_http_match_uri(msg, "/index"))
		handle_index_html(connection, msg);
	else if (mg_http_match_uri(msg, "/favicon.ico") || mg_http_match_uri(msg, "/favicon"))
		handle_favicon_html(connection, msg);
	else if (starts_with(msg->uri.ptr, "/file"))
		handle_file_html(connection, msg);
	else if (starts_with(msg->uri.ptr, "/dir"))
		handle_dir_html(connection, msg);
	else send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
}



void client_handler(struct mg_connection* connection, int ev, void* ev_data, void* fn_data)
{
	if (ev == MG_EV_HTTP_MSG)
	{
		auto* msg = static_cast<mg_http_message*>(ev_data);
		handle_http_message(connection, msg);
	}
}

void server_initialize()
{
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	mg_log_set(log_level);
	mg_mgr_init(&manager);
	
	//	config_initialization_thread();
}

void server_run()
{
	if (!(server_connection = mg_http_listen(&manager, address, client_handler, nullptr)))
	{
		MG_ERROR(("Cannot start listening on %s. Use 'http://ADDR:PORT' or just ':PORT'", address));
		exit(EXIT_FAILURE);
	}
	
	if (hexdump) server_connection->is_hexdumping = 1;
	
	auto cwd = getcwd();
	
	MG_INFO(("Mongoose v" MG_VERSION));
	MG_INFO(("Server listening on : [%s]", address));
	MG_INFO(("Web root directory  : [file://%s/]", cwd));
	
	delete[] cwd;
	
	while (s_signo == 0) mg_mgr_poll(&manager, 1000);
	
	mg_mgr_free(&manager);
	MG_INFO(("Exiting due to signal [%d]...", s_signo));
}



inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	mg_http_reply(
			connection, 200, "Content-Type: text/html\r\n", reinterpret_cast<const char*>(index_html),
			reinterpret_cast<const char*>(article_html)
	);
}


inline void handle_favicon_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	http_send_resource_file(connection, msg, reinterpret_cast<const char*>(favicon_ico), favicon_ico_len);
}


inline void handle_file_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	std::string path;
	
	char* uri = new char[msg->uri.len + 1];
	strncpy(uri, msg->uri.ptr, msg->uri.len);
	uri[msg->uri.len] = 0;
	
	::strscanf(uri, "/file/%s", &path);
	
	delete[] uri;
	
	std::string spath(secure_path(path));
	
	if (!spath.starts_with('/')) spath = "/" + spath;
	spath = getcwd() + spath;
	
	struct stat st{ };
	if (::stat(spath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode))
	{
		struct mg_http_serve_opts opts{ };
		std::string extra_header;
		
		if (st.st_size > MAX_INLINE_FILE_SIZE)
		{
			extra_header = "Content-Disposition: attachment; filename=\"";
			extra_header += path_basename(spath.c_str());
			extra_header += "\"\r\n";
			
			opts.extra_headers = extra_header.c_str();
		}
		
		mg_http_serve_file(connection, msg, spath.c_str(), &opts);
	}
	else send_error_html(connection, 404, "rgba(147, 0, 0, 0.90)");
}


inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg)
{
	std::string path;
	
	char* uri = new char[msg->uri.len + 1];
	strncpy(uri, msg->uri.ptr, msg->uri.len);
	uri[msg->uri.len] = 0;
	
	::strscanf(uri, "/dir/%s", &path);
	
	delete[] uri;
	
	std::string spath(secure_path(path));
	
	if (!spath.starts_with('/')) spath = "/" + spath;
	spath = getcwd() + spath;
	
	struct stat st{ };
	if (::stat(spath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
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
		
		std::string msgstrcp(msg->message.ptr, msg->uri.ptr + msg->uri.len);
		msgstrcp.erase(
				msgstrcp.begin() + (msg->uri.ptr - msg->message.ptr),
				msgstrcp.begin() + (msg->uri.ptr - msg->message.ptr) + 4
		);
		
		struct mg_http_message msg2{ };
		msg2.message = mg_str_n(msgstrcp.data(), msgstrcp.size());
		msg2.uri = mg_str_n(msg2.message.ptr + (msg->uri.ptr - msg->message.ptr), msg->uri.len - 4);
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



inline static consteval size_t static_strlen(const char* str)
{
	size_t len = 0;
	for (; *str; ++len, ++str);
	return len;
}

inline int starts_with(const char* str, const char* prefix)
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