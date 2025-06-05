// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>

/// This file has functions that define the server

#include "constants.h"
#include "settings.h"
#include "server.h"
#include "../mongoose/mongoose.c"
#include "../strscan/strscan.c"
#include "../resources.hpp"
#include "tools.h"
#include "users.h"


#ifdef ENABLE_FILESYSTEM_ACCESS
# include <fineftp/server.h>
#endif


#include <ftw.h>
#include <regex>
#include <curl/curl.h>


//// Default Values for CLI Parameters ////
const char* http_address = DEFAULT_HTTP_SERVER_ADDRESS;
const char* https_address = DEFAULT_HTTPS_SERVER_ADDRESS;
const char* tls_path = nullptr;
const char* server_verification_email = nullptr;
const char* server_verification_email_password = nullptr;
const char* server_verification_smtp_server = "smtps://smtp.gmail.com:465";
int log_level = 2, hexdump = 0;
//// ////

#ifdef ENABLE_FILESYSTEM_ACCESS
pthread_mutex_t ftp_callback_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Server Connection Manager
static struct mg_mgr manager{};
// The connection itself
static struct mg_connection *http_server_connection = nullptr, *https_server_connection = nullptr;

#ifdef ENABLE_FILESYSTEM_ACCESS
// FTP Server Instance
static fineftp::FtpServer ftp_server;
#endif

// Handle interrupts, like Ctrl-C
static int s_signo = 0;

/// Print detailed info about the signals and handle them as usual
static void signal_handle_print_details(int signo)
{
    MG_ERROR(("[SIGNAL_HANDLER] Handling SIG%s(%d) \"%s\"...", sigabbrev_np(signo), signo, sigdescr_np(signo)));
    signal(signo, SIG_DFL); // Continue as default
    s_signo = signo;
}


/// A linked list of path handlers
typedef struct registered_path_handlers
{
    registered_path_handler* data = nullptr;
    struct registered_path_handlers* next = nullptr;
} registered_path_handlers;

static registered_path_handlers* handlers_start = nullptr;
static registered_path_handlers* handlers_head = nullptr;


/// Create /etc/webserver directory if does not exist
inline void init_config_dir();


/// Send icon resource string as regular file over http
inline void http_send_resource(
    struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize,
    const char* mime_type
);

/// Send error page over http
inline void send_error_html(struct mg_connection* connection, int code, const char* color, const char* msg);


/// Handle index page access
inline void handle_index_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle favicon access
inline void handle_favicon_ico(struct mg_connection* connection, struct mg_http_message* msg);

#ifdef ENABLE_FILESYSTEM_ACCESS

/// Handle filesystem access (mongoose default serve directory)
inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg);

#endif

/// Handle user registration form GET access
inline void handle_register_form_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user registration POST request
inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle user email verification request
inline void handle_verify_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Handle other resources request
inline void handle_resources_html(struct mg_connection* connection, struct mg_http_message* msg);

/// Add path handler to global linked list
void register_path_handler(const std::string& path, const std::string& description, path_handler_function fn)
{
    MG_DEBUG(("Registering '%s' => '%s' path...", description.c_str(), path.c_str()));
    if (!handlers_head)
    {
        // If no entries - create one
        handlers_start = new registered_path_handlers{
            .data = new registered_path_handler{
                .path_regex = path, .description = description, .fn = fn
            },
            .next = nullptr
        };
        handlers_head = handlers_start;
    }
    else
    {
        // Otherwise, create a new entry after the head of the list
        handlers_head->next = new registered_path_handlers{
            .data = new registered_path_handler{
                .path_regex = path, .description = description, .fn = fn
            },
            .next = nullptr
        };
        // And move the head of the list
        handlers_head = handlers_head->next;
    }
}

/// Iterate through registered handlers_start and try handle them
inline void handle_registered_paths(struct mg_connection* connection, struct mg_http_message* msg)
{
    MG_DEBUG(("Handling non-builtin registered paths..."));
    registered_path_handlers* root = handlers_start;

    while (root && root->data)
    {
        if (mg_match(msg->uri, mg_str(root->data->path_regex.c_str()), nullptr))
            break; // This is the one
        root = root->next;
    }

    if (root && root->data) // If found one
    {
        MG_DEBUG(("Handling '%s' => '%s' path for IP %M...", root->data->description.c_str(), root->data->path_regex.c_str(),
            mg_print_ip, &connection->rem));
        return root->data->fn(connection, msg); // Run the handler and quit after
    }

    send_error_html(connection, COLORED_ERROR(404), "");
}

// Serve appropriate resources and perform actions upon client's request
inline void handle_http_message(struct mg_connection* connection, struct mg_http_message* msg)
{
    // Serve index.html resource on '/', '/index.html'
    if (mg_match(msg->uri, _MATCH_CSTR("/")) ||
        mg_match(msg->uri, _MATCH_CSTR("/index.html")))
        handle_index_html(connection, msg);
    else if (mg_match(msg->uri, _MATCH_CSTR("/favicon.ico"))) // Serve favicon.ico resource on '/favicon.ico'
        handle_favicon_ico(connection, msg);
#ifdef ENABLE_FILESYSTEM_ACCESS // If FS access is enabled...
    else if (mg_match(msg->uri, _MATCH_CSTR("/dir/#"))) // Let the user browse directories on '/dir/...'
        handle_dir_html(connection, msg);
#endif
    else if (mg_match(msg->uri, _MATCH_CSTR("/register-form"))) // Serve register.html on '/register-form'
        handle_register_form_html(connection, msg);
    else if (mg_match(msg->uri, _MATCH_CSTR("/register"))) // Receive data from registration form
        handle_register_html(connection, msg);
    else if (mg_match(msg->uri, _MATCH_CSTR("/verify/*"))) // Verify email address and serve verify.html
        handle_verify_html(connection, msg);
    else if (mg_match(msg->uri, _MATCH_CSTR("/resources/#"))) // Serve built-in resource files
        handle_resources_html(connection, msg);
    else handle_registered_paths(connection, msg); // Handle other paths registered in [config.cpp]
}


/// Handle mongoose events
void client_handler(struct mg_connection* connection, int ev, void* ev_data)
{
    if (connection->fn_data != nullptr && ev == MG_EV_ACCEPT) // When user starts a session
    {
        std::string stls_path(tls_path); // TLS Certificate and Key folder
        if (!stls_path.ends_with('/')) stls_path += '/'; // always '/' at the end

        std::string cert_path(stls_path); // Public Certificate
        cert_path += "cert.pem";

        std::string key_path(stls_path); // Private Key
        key_path += "key.pem";

        MG_DEBUG(("Reading [%.*s]...", cert_path.size(), cert_path.c_str()));
        std::string Cert_ = FILE_read_all(cert_path);
        MG_DEBUG(("Read: { %.*s... }", 25, Cert_.c_str()));

        MG_DEBUG(("Reading [%.*s]...", key_path.size(), key_path.c_str()));
        std::string Key_ = FILE_read_all(key_path);
        MG_DEBUG(("Read: { %.*s... }", 15, Key_.c_str()));

        // Make a structure to pass to mongoose
        struct mg_tls_opts opts = {
            .cert = mg_str(Cert_.c_str()),
            .key = mg_str(Key_.c_str()),
        };
        mg_tls_init(connection, &opts); // Initialize TLS Connection
    }
    else if (ev == MG_EV_HTTP_MSG) // When user requests pages and other data
    {
        auto* msg = static_cast<mg_http_message*>(ev_data);
        handle_http_message(connection, msg);
    }
}


inline void init_config_dir()
{
    mkdir_p(CONFIG_DIR);
}

/// Initialize server
void server_initialize()
{
    // Create config directory - if does not exist
    init_config_dir();

    // If the email has been defined, but the password is empty
    if (server_verification_email != nullptr && server_verification_email_password == nullptr)
    {
        puts("[Server] An error occurred during server initialization: No password was given for verification email.");
        exit(-3);
    }

    // If no email has been defined
    if (server_verification_email == nullptr || *server_verification_email == 0) // <- terminating character
    {
        // Print a warning
        puts("[Server] ======= HEADS UP ======");
        puts("[Server] Verification email was not configured. Entering unsafe mode!!!");
        puts("[Server] !!! Heads up! Your server is vulnerable to spam attacks! !!!");
        puts("[Server] Please, consider creating a google account and set it up as verification email.");
        puts("[Server] ======= HEADS UP ======");

        // If still not nullptr - delete it
        delete[] server_verification_email;
        delete[] server_verification_email_password;
        // And set to nullptr
        server_verification_email = server_verification_email_password = nullptr;
    }

    // Initialize libcurl library
    curl_global_init(CURL_GLOBAL_ALL);

    // Handle SIGNALS properly
    signal(SIGINT, signal_handle_print_details);
    signal(SIGTERM, signal_handle_print_details);
    signal(SIGQUIT, signal_handle_print_details);

    mg_log_set(log_level); // Set log level for mongoose
    mg_mgr_init(&manager); // Initialize mongoose

#ifdef ENABLE_FILESYSTEM_ACCESS
    if (log_level >= MG_LL_INFO)
    {
        // Print all ftp commands in logs
        add_custom_ftp_handler(
            [](
            const std::string& ftp_command, const std::string& parameters, const std::string& ftp_working_directory,
            std::shared_ptr<::fineftp::FtpUser> ftp_user
        )
            {
                mutex_locker l(&ftp_callback_mutex); // Async
                MG_DEBUG((" [FTP] %s %s", ftp_command.c_str(), parameters.c_str()));
            }
        );
    }
#endif

#ifdef ENABLE_FILESYSTEM_ACCESS
    register_additional_handlers(); // from config.cpp

    // Anonymous user can view everyone's files, but not edit
    MG_DEBUG(("[FTP] Adding anonymous user to ftp server..."));
    ftp_server.addUserAnonymous(getcwd(), fineftp::Permission::ReadOnly);
#endif

    load_users();

#ifdef ENABLE_FILESYSTEM_ACCESS
    MG_DEBUG(("[FTP] Forwarding users to ftp server..."));
    forward_users(ftp_server);
#endif
}

/// Start listening on given http_address and run server loop
void server_run()
{
    if (http_address)
    {
        if (!(http_server_connection = mg_http_listen(&manager, http_address, client_handler, nullptr)))
        {
            MG_ERROR(("Could not start listening on '%s'. Use 'http://ADDR:PORT' or just ':PORT' as http(s) address parameter", http_address));
            exit(EXIT_FAILURE);
        }
    }

    if (tls_path && https_address)
    {
        if (!(https_server_connection = mg_http_listen(&manager, https_address, client_handler, (void*)1)))
        {
            MG_ERROR(("Could not start listening on %s. Use 'https://ADDR:PORT' or just ':PORT' as https address parameter", https_address));
            https_server_connection = nullptr;
        }
    }

    // Override HexDumping for http server
    if (http_server_connection)
        http_server_connection->is_hexdumping = hexdump;

    // Override HexDumping for https server
    if (https_server_connection)
        https_server_connection->is_hexdumping = hexdump;

    auto cwd = getcwd(); // Current Working Directory


    if (http_server_connection) // HTTP
    {
        MG_INFO((""));
        MG_INFO(("Mongoose v" MG_VERSION));
        MG_INFO(("Server is listening on : [%s]", http_address));
        MG_INFO(("Web root directory  : [file://%s/]", cwd.c_str()));
        MG_INFO((""));
    }

    if (https_server_connection) // HTTPS
    {
        MG_INFO((""));
        MG_INFO(("Mongoose v" MG_VERSION));
        MG_INFO(("Server is listening on : [%s]", https_address));
        MG_INFO(("Web root directory  : [file://%s/]", cwd.c_str()));
        MG_INFO(("Certificates directory  : [file://%s/]", tls_path));
        MG_INFO((""));
    }

#ifdef ENABLE_FILESYSTEM_ACCESS // FTP Server
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
    MG_DEBUG(("Serving index.html to %M...", mg_print_ip, &connection->rem));

    std::string list_html;
#ifdef ENABLE_FILESYSTEM_ACCESS // if filesystem is enabled
    list_html += "<li><a href=\"/dir/\">Observe directory structure</a></li>\n"; // Add '/dir/' link
#endif
    list_html += "<li><a href=\"/register-form\">Create a new account</a></li>\n"; // Add register link
    // List other path handlers defined in config.cpp
    for (auto* i = handlers_start; i != nullptr && i->data != nullptr; i = i->next)
    {
        list_html += "<li><a href=\"" + i->data->path_regex + "\">" + i->data->description + "</a></li>\n";
        MG_DEBUG(("Indexed '%s' => '%s'.", i->data->description.c_str(), i->data->path_regex.c_str()));
    }

    // Add it to the article
    char article_complete[LEN(article_html) + list_html.size() + 1];
    sprintf(article_complete, RESOURCE(article_html), list_html.c_str());

    // Send...
    mg_http_reply(
        connection, 200, "Content-Type: text/html\r\n",
        RESOURCE(index_html), article_complete
    );
}


inline void handle_favicon_ico(struct mg_connection* connection, struct mg_http_message* msg)
{
    MG_DEBUG(("Serving favicon.ico to %M...", mg_print_ip, &connection->rem));
    // Send...
    http_send_resource(connection, msg, RESOURCE(favicon_ico), LEN(favicon_ico), "image/x-icon");
}


#ifdef ENABLE_FILESYSTEM_ACCESS

static void list_dir(struct mg_connection* c, struct mg_http_message* hm, const struct mg_http_serve_opts* opts,
                     char* dir)
{
    ///
    /// This is a copy of mongoose's built-in function <br>
    /// mg_http_serve_dir() -> <b>listdir()</b> <br>
    /// I don't guarantee the correctness of this function, because I just copied it :)
    ///

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
    int len = mg_url_decode(_EXPAND(hm->uri), buf, sizeof(buf), 0);
    struct mg_str uri = len > 0 ? mg_str_n(buf, (size_t)len) : hm->uri;

    mg_printf(
        c,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "%s"
        "Content-Length:         \r\n\r\n",
        opts->extra_headers == nullptr ? "" : opts->extra_headers
    );
    off = c->send.len; // Start of body
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
        _PRINT(uri), sort_js_code, sort_js_code2, _PRINT(uri)
    );
    mg_printf(
        c, "%s",
        "  <tr><td><a href=\"..\">..</a></td>"
        "<td name=-1></td><td name=-1>[DIR]</td></tr>\n"
    );

    fs->ls(dir, printdirentry, &d);
    mg_printf(c, "</tbody><tfoot><tr><td colspan=\"3\"><hr></td></tr></tfoot> </table></body></html>\n");
    // The purpose of the copy-paste is to remove <address>Mongoose v....</address> up here ^
    n = mg_snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)(c->send.len - off));
    if (n > sizeof(tmp)) n = 0;
    memcpy(c->send.buf + off - 12, tmp, n); // Set content length
    c->is_resp = 0; // Mark response end
}

void serve_dir(struct mg_connection* c, struct mg_http_message* hm, const struct mg_http_serve_opts* opts)
{
    ///
    /// This is a copy of mongoose's built-in function <b>mg_http_serve_dir()</b> <br>
    /// I don't guarantee the correctness of this function, because I just copied it :)
    ///

    char path[MG_PATH_MAX];
    const char* sp = opts->ssi_pattern;
    int flags = uri_to_path(c, hm, opts, path, sizeof(path));
    if (flags < 0)
    {
        // Do nothing: the response has already been sent by uri_to_path()
    }
    else if (flags & MG_FS_DIR)
    {
        list_dir(c, hm, opts, path);
    }
    else if (flags && sp != nullptr && mg_match(mg_str(path), mg_str(sp), nullptr))
    {
        mg_http_serve_ssi(c, opts->root_dir, path);
    }
    else
    {
        mg_http_serve_file(c, hm, path, opts);
    }
}

inline void handle_dir_html(struct mg_connection* connection, struct mg_http_message* msg)
{
    MG_DEBUG(("Serving /dir/ to %M...", mg_print_ip, &connection->rem));
    char* path = nullptr;
    auto cwd = getcwd(); // Current Working Directory

    std::string uri(_EXPAND(msg->uri));
    strscanf(uri.c_str(), "/dir/%s", &path); // Extract path from uri

    std::string spath(secure_path(path ? path : "")); // PATH string
    delete[] path;

    if (!spath.starts_with('/')) spath = "/" + spath; // Make always start with a '/'

    std::string sdpath(spath);
    sdpath.reserve(sdpath.size() + 1); // Set string buffer size
    // Decode %XX encoded characters (makes it smaller, so won't overflow)
    mg_url_decode(sdpath.c_str(), sdpath.size(), sdpath.data(), sdpath.size() + 1, 0);
    sdpath = sdpath.data(); // trim off data after '\0' left from overwriting itself

    struct stat st{};
    if (::stat((cwd + sdpath).c_str(), &st) != 0) // Check if path exists
    {
        send_error_html(connection, COLORED_ERROR(404), ""); // If no - error 404
        return;
    }

    // options for function serve_dir()
    struct mg_http_serve_opts opts{.root_dir = cwd.c_str()};
    std::string extra_header;

    // If file is too big - serve as an attachment
    if (st.st_size > MAX_INLINE_FILE_SIZE)
    {
        extra_header = "Content-Disposition: attachment; filename=\"";
        extra_header += path_basename(sdpath);
        extra_header += "\"\r\n";

        opts.extra_headers = extra_header.c_str();
    }

    // Copy everything up to uri
    std::string msgstrcp(msg->message.buf, msg->uri.buf);
    msgstrcp += spath;

    // Create fabricated http message for serve_dir()
    struct mg_http_message msg2{};
    msg2.message = mg_str_n(msgstrcp.data(), msgstrcp.size());
    msg2.uri = mg_str_n(msg2.message.buf + (msg->uri.buf - msg->message.buf), spath.size());
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
    MG_DEBUG(("Serving /register-form to %M...", mg_print_ip, &connection->rem));
    mg_http_reply(connection, 200, "Content-Type: text/html\r\n", RESOURCE(register_html));
}


inline void send_verification_notification_page_html(struct mg_connection* connection)
{
    MG_DEBUG(("Sending email verification page to %M...", mg_print_ip, &connection->rem));
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

inline id_t generate_id_and_send_email(struct mg_connection* connection, struct mg_http_message* msg,
                                       const std::string& email)
{
    MG_DEBUG(("[Send Email] Checking user email..."));
    auto email_hostaddr_pos = email.find('@');
    if (email_hostaddr_pos == std::string::npos)
        return 0;

    if (!server_verification_email_hosts_whitelist.empty() &&
        !server_verification_email_hosts_whitelist.contains(email.substr(email_hostaddr_pos + 1)))
    {
        MG_DEBUG(("[Send Email] Host Not Allowed: %s", email.c_str()[email_hostaddr_pos + 1]));
        send_error_html(connection, COLORED_ERROR(406), "This email service provider is not allowed");
        return 0;
    }

    if (!server_verification_email_hosts_blacklist.empty() &&
        server_verification_email_hosts_blacklist.contains(email.substr(email_hostaddr_pos + 1)))
    {
        MG_DEBUG(("[Send Email] Banned Host: %s", email.c_str()[email_hostaddr_pos + 1]));
        send_error_html(connection, COLORED_ERROR(406), "This email service provider is not allowed");
        return 0;
    }

    auto server_address = mg_http_get_header(msg, "Host");

    if (server_address == nullptr)
    {
        MG_DEBUG(("[Send Email] Can't obtain Host address from header: 'Host'"));
        send_error_html(connection, COLORED_ERROR(500), "Can't generate a verification link");
        return 0;
    }


    MG_DEBUG(("[Send Email] Generating ID..."));

    std::string server_address_str(server_address->buf, server_address->len);

    srandom(mg_millis());

    id_t id = random();
    for (int i = 0; id == 0 || pending_id_exists(id) && i < 10; ++i) id = random();
    if (pending_id_exists(id))
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

    MG_DEBUG(("Sending link [%s] to [%s]", link.c_str(), email.c_str()));

    struct curl_slist* recipients = nullptr;
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, server_verification_email);
    recipients = curl_slist_append(recipients, email.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_callback_email_data);
    MessageData data{.message = message, .pos = 0};
    curl_easy_setopt(curl, CURLOPT_READDATA, &data);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // Send the email
    CURLcode res = curl_easy_perform(curl);

    // Clean up
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    // Check for errors
    if (res != CURLE_OK)
    {
        MG_ERROR(("[Send Email] curl_easy_perform() failed: %s\n", curl_easy_strerror(res)));
        send_error_html(connection, COLORED_ERROR(500), "Can't generate a verification link");
        return 0;
    }

    return id;
}

inline void handle_register_html(struct mg_connection* connection, struct mg_http_message* msg)
{
    MG_DEBUG(("Processing registration request from %M...", mg_print_ip, &connection->rem));
    if (mg_strcmp(msg->method, mg_str("POST")))
    {
        MG_ERROR(("Error parsing request: Invalid method: [%.*s]", _PRINT(msg->method)));
        return;
    }

    char login[HOST_NAME_MAX]{}, email[HOST_NAME_MAX]{}, password[HOST_NAME_MAX]{};
    mg_http_get_var(&msg->body, "login", login, sizeof(login));
    mg_http_get_var(&msg->body, "email", email, sizeof(email));
    mg_http_get_var(&msg->body, "password", password, sizeof(password));

    if (login[0] == 0)
    {
        MG_DEBUG(("Login.size = 0"));
        send_error_html(connection, COLORED_ERROR(406), "Login is required");
        return;
    }

    if (email[0] == 0)
    {
        MG_DEBUG(("Email.size = 0"));
        send_error_html(connection, COLORED_ERROR(406), "Email is required");
        return;
    }

    if (password[0] == 0 || strlen(password) < 8)
    {
        MG_DEBUG(("Password.size = 0"));
        send_error_html(connection, COLORED_ERROR(406), "Password is required and must be at least 8 characters long");
        return;
    }

    if (get_registered_users()->contains(login))
    {
        send_error_html(connection, COLORED_ERROR(409), "User already exists");
        MG_INFO(("Blocked an attempt to create existing user - '%s'.", login));
        return;
    }

    if (!std::regex_match(login, std::regex(REGEX_LOGIN)) || !std::regex_match(email, std::regex(REGEX_EMAIL)))
    {
        send_error_html(connection, COLORED_ERROR(406), "Wrong login or email format");
        MG_INFO(("Blocked an attempt to create user - '%s' / '%s' / '%s'.", login, email, password));
        return;
    }

    if (std::string(password).find(' ') != std::string::npos)
    {
        send_error_html(connection, COLORED_ERROR(406), "The password should not contain whitespaces");
        MG_INFO(("Blocked an attempt to create user - '%s' / '%s' / '%s'.", login, email, password));
        return;
    }

    if (server_verification_email == nullptr)
    {
        MG_DEBUG(("Verification Email --- Enabled"));
        __user_map_t::value_type user{login, {email, password}};
        if (!add_new_user(user))
        {
            send_error_html(connection, COLORED_ERROR(500), "Could not insert user");
            return;
        }
#ifdef ENABLE_FILESYSTEM_ACCESS
        add_user(ftp_server, user);
#endif
        mg_http_reply(connection, 200, "", "Success"); // TODO: Successful Creation Page
    }
    else
    {
        MG_DEBUG(("Verification Email --- Disabled"));
        id_t id = generate_id_and_send_email(connection, msg, email);
        if (id == 0) return;

        add_pending_user(id, {login, {email, password}});
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

    MG_DEBUG(("Processing verification request from %M...", mg_print_ip, &connection->rem));
    if (mg_strcmp(msg->method, mg_str("GET")))
    {
        MG_ERROR(("Error parsing request: Invalid method: [%.*s]", _PRINT(msg->method)));
        return;
    }

    char* id_ptr = nullptr;

    std::string uri(_EXPAND(msg->uri));
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

    auto pending_user = find_pending_user(id);
    if (pending_user == pending_user_invalid())
    {
        send_error_html(connection, COLORED_ERROR(403), "Invalid link");
        return;
    }

    add_new_user(pending_user->second);
#ifdef ENABLE_FILESYSTEM_ACCESS
    add_user(ftp_server, pending_user->second);
#endif

    mg_http_reply(connection, 200, "", "Success");
}


inline void handle_resources_html(struct mg_connection* connection, struct mg_http_message* msg)
{
    MG_DEBUG(("Processing /resources/ for %M...", mg_print_ip, &connection->rem));

    char* path = nullptr;

    std::string uri(_EXPAND(msg->uri));
    strscanf(uri.c_str(), "/resources/%s", &path);

    if (path == nullptr) // Case: /resources/
    {
        send_error_html(connection, COLORED_ERROR(400), "No resource given");
        return;
    }

    std::string path_s(path);
    delete[] path;

    if (path_s == "bootstrap.css") // Case: /resources/bootstrap.css
        http_send_resource(connection, msg, RESOURCE(bootstrap_css), LEN(bootstrap_css), "text/css");
    else if (path_s == "CascadiaMono.woff") // Case: /resources/CascadiaMono.woff
        http_send_resource(connection, msg, RESOURCE(CascadiaMono_woff), LEN(CascadiaMono_woff), "font/woff");
    else
        send_error_html(connection, COLORED_ERROR(501), "This resource does not exist");
}

inline void http_send_resource(struct mg_connection* connection, struct mg_http_message* msg, const char* rcdata, size_t rcsize,
                               const char* mime_type)
{
    int n, status = 200;
    char range[100]{};
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
        _PRINT(mime),
        cl,
        range
    );

    if (mg_strcasecmp(msg->method, mg_str("HEAD")) == 0)
    {
        connection->is_draining = 1;
        connection->is_resp = 0;

        MG_ERROR(("Error parsing request: Invalid method: [%.*s]", _PRINT(msg->method)));
        return;
    }


    typedef struct
    {
        const char* data;
        uint64_t len;
        uint64_t pos;
    } str_buf_fd;

    connection->pfn = [](struct mg_connection* c, int ev, void* ev_data)
    {
        if (ev == MG_EV_WRITE || ev == MG_EV_POLL)
        {
            auto rc = (str_buf_fd*)c->pfn_data;

            // Read to send IO buffer directly, avoid extra on-stack buffer
            size_t max = MG_IO_SIZE, space;
            auto* cl = reinterpret_cast<size_t*>(&c->data[(sizeof(c->data) - sizeof(size_t)) /
                sizeof(size_t) * sizeof(size_t)]);
            if (c->send.size < max)
                mg_iobuf_resize(&c->send, max);
            if (c->send.len >= c->send.size)
                return; // Rate limit
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
    auto clp = reinterpret_cast<size_t*>(&connection->data[(sizeof(connection->data) - sizeof(size_t)) /
        sizeof(size_t) * sizeof(size_t)]);
    connection->pfn_data = new str_buf_fd{.data = rcdata, .len = rcsize, .pos = 0};
    *clp = cl; // Track to-be-sent content length
}


inline void send_error_html(struct mg_connection* connection, int code, const char* color, const char* msg)
{
    MG_DEBUG(("Sending error message: Error %d \"%s\"...", code, msg));
    mg_http_reply(
        connection, code, "Content-Type: text/html\r\n", RESOURCE(error_html),
        color, color, color, color, code, mg_http_status_code_str(code), msg
    );
}
