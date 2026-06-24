#ifndef SCL_HTTP_SERVER_H
#define SCL_HTTP_SERVER_H

/*
 * scl_http_server — a small, security-first HTTP/1.1 server built on the
 * lock-free scl_tcp_pool. Design priorities, in order: memory safety on
 * untrusted input, correct request framing (no smuggling), then throughput.
 *
 * Threading model:
 *   - one acceptor thread accept()s and hands the connection to the pool's
 *     lock-free MPMC ready-queue, then wakes a worker;
 *   - N worker threads pull from the ready-queue and service the connection
 *     (keep-alive capable), then return the slot to the pool.
 * The data path (acquire/post/get/release) is lock-free; a condvar semaphore
 * is used only to park idle workers.
 *
 * Static files are served from a docroot with strict path-traversal defense
 * (percent-decode, reject "..", realpath() containment). A single optional
 * application handler may pre-empt static serving for dynamic routes.
 */

#include "scl_common.h"
#include "scl_tcp_pool.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define SCL_HTTP_MAX_HEADERS 64

/* ── Parsed request (valid only inside a handler call) ─────────── */
typedef struct {
    const char *name;   /* NUL-terminated, in the connection buffer */
    const char *value;  /* NUL-terminated */
} scl_http_header_t;

typedef struct {
    const char       *method;       /* e.g. "GET" */
    const char       *target;       /* raw request-target incl. query */
    const char       *path;         /* percent-decoded path, no query */
    int               version_minor;/* 0 => HTTP/1.0, 1 => HTTP/1.1 */
    bool              keep_alive;
    size_t            header_count;
    scl_http_header_t headers[SCL_HTTP_MAX_HEADERS];
} scl_http_request_t;

/* Case-insensitive header lookup; returns NULL if absent. */
const char *scl_http_request_header(const scl_http_request_t *req, const char *name);

/* ── Response the application handler fills in ─────────────────── */
typedef struct {
    int          status;        /* e.g. 200 */
    const char  *content_type;  /* e.g. "application/json" */
    const void  *body;          /* bytes to send (must outlive the send) */
    size_t       body_len;
    bool         close;         /* force Connection: close */
} scl_http_response_t;

/* Application handler. Return true if it produced a response (req fully
 * handled); false to fall through to static file serving. */
typedef bool (*scl_http_handler_fn)(const scl_http_request_t *req,
                                    scl_http_response_t *resp, void *user);

/* ── Configuration ─────────────────────────────────────────────── */
typedef struct {
    const char *host;             /* bind host, NULL => all interfaces */
    uint16_t    port;             /* 0 => ephemeral (query with _port()) */
    const char *docroot;          /* static root; NULL => no static serving */
    int         num_workers;      /* worker threads (default 4) */
    size_t      pool_capacity;    /* max concurrent connections (default 256) */
    size_t      conn_buf_cap;     /* per-conn buffer / max request bytes (default 64K) */
    int64_t     recv_timeout_ms;  /* idle/recv timeout (default 15000) */
    int         backlog;          /* listen backlog (default SOMAXCONN) */
    int         keep_alive_max;   /* max requests per connection (default 100) */
    const char *server_name;      /* Server: header (default "scl-httpd") */
    scl_http_handler_fn handler;  /* optional dynamic handler */
    void       *handler_user;     /* passed to handler */
} scl_http_config_t;

typedef struct scl_http_server scl_http_server_t;

/* Allocate/initialize. Binds the listening socket (so port 0 resolves now). */
scl_error_t scl_http_server_init(scl_allocator_t *alloc, scl_http_server_t **out,
                                 const scl_http_config_t *cfg);

/* Spawn acceptor + workers. Non-blocking: returns once threads are running. */
scl_error_t scl_http_server_start(scl_http_server_t *srv);

/* Signal shutdown and join all threads. Idempotent. */
scl_error_t scl_http_server_stop(scl_http_server_t *srv);

/* Actual bound port (useful when configured with port 0). */
uint16_t scl_http_server_port(const scl_http_server_t *srv);

void scl_http_server_destroy(scl_http_server_t *srv);

/* Extension -> MIME type (e.g. "html" -> "text/html"). Never NULL. */
const char *scl_http_mime_for_ext(const char *ext);

#endif /* SCL_HTTP_SERVER_H */
