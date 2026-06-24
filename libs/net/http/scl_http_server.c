#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O2")
#endif

#include "scl_http_server.h"
#include "scl_tcp_pool.h"
#include "scl_string.h"
#include "scl_stdlib.h"
#include "scl_pthread.h"
#include "scl_time.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define SCL_HTTP_MAX_TARGET  8190
#define SCL_HTTP_SEND_CHUNK  (64 * 1024)

/* ── Server object ──────────────────────────────────────────── */
struct scl_http_server {
    scl_allocator_t   *alloc;
    scl_http_config_t  cfg;
    char               server_name[64];
    char               docroot_real[SCL_PATH_MAX];
    size_t             docroot_real_len;

    int                listen_fd;
    uint16_t           bound_port;

    scl_tcp_pool_t     pool;

    scl_thread_t       acceptor;
    scl_thread_t      *workers;
    int                num_workers;

    /* condvar semaphore that parks idle workers */
    scl_mutex_t        sem_m;
    scl_cond_t         sem_c;
    int                sem_count;

    atomic_bool        running;
    atomic_bool        started;
};

/* ── MIME table ─────────────────────────────────────────────── */
static const struct { const char *ext, *type; } scl_mime_table[] = {
    { "html", "text/html; charset=utf-8" },
    { "htm",  "text/html; charset=utf-8" },
    { "css",  "text/css; charset=utf-8" },
    { "js",   "text/javascript; charset=utf-8" },
    { "mjs",  "text/javascript; charset=utf-8" },
    { "json", "application/json" },
    { "xml",  "application/xml" },
    { "txt",  "text/plain; charset=utf-8" },
    { "csv",  "text/csv; charset=utf-8" },
    { "md",   "text/markdown; charset=utf-8" },
    { "pdf",  "application/pdf" },
    { "png",  "image/png" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "gif",  "image/gif" },
    { "webp", "image/webp" },
    { "svg",  "image/svg+xml" },
    { "ico",  "image/x-icon" },
    { "bmp",  "image/bmp" },
    { "woff", "font/woff" },
    { "woff2","font/woff2" },
    { "ttf",  "font/ttf" },
    { "otf",  "font/otf" },
    { "wasm", "application/wasm" },
    { "mp4",  "video/mp4" },
    { "webm", "video/webm" },
    { "mp3",  "audio/mpeg" },
    { "wav",  "audio/wav" },
    { "zip",  "application/zip" },
    { "gz",   "application/gzip" },
    { "tar",  "application/x-tar" },
};

const char *scl_http_mime_for_ext(const char *ext) {
    if (ext) {
        for (size_t i = 0; i < SCL_ARRAY_SIZE(scl_mime_table); i++)
            if (scl_strcmp(ext, scl_mime_table[i].ext) == 0)
                return scl_mime_table[i].type;
    }
    return "application/octet-stream";
}

static const char *mime_for_path(const char *path) {
    const char *dot = scl_strrchr(path, '.');
    const char *slash = scl_strrchr(path, '/');
    if (!dot || (slash && dot < slash) || dot[1] == '\0')
        return "application/octet-stream";
    char ext[16];
    size_t n = 0;
    for (const char *p = dot + 1; *p && n < sizeof(ext) - 1; p++)
        ext[n++] = (char)scl_tolower((unsigned char)*p);
    ext[n] = '\0';
    return scl_http_mime_for_ext(ext);
}

/* ── Status text ────────────────────────────────────────────── */
static const char *status_text(int code) {
    switch (code) {
    case 200: return "OK";
    case 206: return "Partial Content";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 413: return "Payload Too Large";
    case 414: return "URI Too Long";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 505: return "HTTP Version Not Supported";
    default:  return "Error";
    }
}

/* ── Header lookup (case-insensitive) ───────────────────────── */
static int ci_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (scl_tolower((unsigned char)*a) != scl_tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

const char *scl_http_request_header(const scl_http_request_t *req, const char *name) {
    if (!req || !name) return NULL;
    for (size_t i = 0; i < req->header_count; i++)
        if (ci_equal(req->headers[i].name, name))
            return req->headers[i].value;
    return NULL;
}

static int ci_contains_token(const char *haystack, const char *token) {
    /* crude case-insensitive substring (header values are short) */
    if (!haystack) return 0;
    size_t tl = scl_strlen(token);
    for (const char *p = haystack; *p; p++) {
        size_t i = 0;
        while (i < tl && p[i] &&
               scl_tolower((unsigned char)p[i]) == scl_tolower((unsigned char)token[i]))
            i++;
        if (i == tl) return 1;
    }
    return 0;
}

/* ── Date header ────────────────────────────────────────────── */
static void http_date(char *out, size_t cap) {
    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    /* RFC 7231 IMF-fixdate, e.g. "Tue, 24 Jun 2026 05:10:00 GMT" */
    strftime(out, cap, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
}

/* ── Percent-decode a path; rejects encoded NUL and bad escapes ─ */
static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Returns decoded length, or -1 on malformed input / embedded NUL. */
static long url_decode_path(const char *src, size_t srclen, char *dst, size_t dstcap) {
    size_t o = 0;
    for (size_t i = 0; i < srclen; i++) {
        char c = src[i];
        if (c == '%') {
            if (i + 2 >= srclen) return -1;
            int hi = hexval((unsigned char)src[i + 1]);
            int lo = hexval((unsigned char)src[i + 2]);
            if (hi < 0 || lo < 0) return -1;
            int v = (hi << 4) | lo;
            if (v == 0) return -1;                 /* reject %00 */
            c = (char)v;
            i += 2;
        }
        if (c == '\0') return -1;
        if (o + 1 >= dstcap) return -1;            /* too long */
        dst[o++] = c;
    }
    dst[o] = '\0';
    return (long)o;
}

/* ── Path-traversal-safe resolution within docroot ──────────────
 * Returns 0 and fills `resolved` on success; otherwise an HTTP status
 * (404/403). Defeats "..", absolute escapes and symlink escapes via
 * realpath() containment. */
static int resolve_in_docroot(const scl_http_server_t *srv, const char *decoded_path,
                              char *resolved, size_t cap) {
    if (srv->docroot_real_len == 0) return 404;
    if (decoded_path[0] != '/') return 400;

    /* Defensive reject of any ".." path segment before touching the FS. */
    for (const char *p = decoded_path; *p; ) {
        if (p[0] == '.' && p[1] == '.' &&
            (p[2] == '/' || p[2] == '\0') &&
            (p == decoded_path || p[-1] == '/'))
            return 403;
        const char *slash = scl_strchr(p, '/');
        p = slash ? slash + 1 : p + scl_strlen(p);
    }

    char candidate[SCL_PATH_MAX];
    int n = snprintf(candidate, sizeof(candidate), "%s%s",
                     srv->docroot_real, decoded_path);
    if (n < 0 || (size_t)n >= sizeof(candidate)) return 414;

    char real[SCL_PATH_MAX];
    if (!realpath(candidate, real)) return 404;    /* nonexistent or unreadable */

    /* Containment: real must be docroot_real itself or live beneath it. */
    size_t rl = scl_strlen(real);
    if (rl < srv->docroot_real_len) return 403;
    if (scl_memcmp(real, srv->docroot_real, srv->docroot_real_len) != 0) return 403;
    if (rl > srv->docroot_real_len && real[srv->docroot_real_len] != '/') return 403;

    if (rl + 1 > cap) return 414;
    scl_memcpy(resolved, real, rl + 1);
    return 0;
}

/* ── Response writing ───────────────────────────────────────── */
static scl_error_t send_simple(const scl_http_server_t *srv, int fd, int status,
                               const char *content_type, const void *body,
                               size_t body_len, bool head_only, bool close_conn) {
    char date[64];
    http_date(date, sizeof(date));
    char hdr[640];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status, status_text(status), date, srv->server_name,
        content_type ? content_type : "text/plain; charset=utf-8",
        body_len, close_conn ? "close" : "keep-alive");
    if (hn < 0 || (size_t)hn >= sizeof(hdr)) return SCL_ERR_IO;

    scl_error_t err = scl_tcp_send_all(fd, hdr, (size_t)hn);
    if (err != SCL_OK) return err;
    if (!head_only && body && body_len)
        err = scl_tcp_send_all(fd, body, body_len);
    return err;
}

/* Canned error page (also serves as the body). */
static scl_error_t send_error(const scl_http_server_t *srv, int fd, int status,
                              bool head_only, bool close_conn) {
    char body[128];
    int bn = snprintf(body, sizeof(body),
        "<html><head><title>%d %s</title></head>"
        "<body><h1>%d %s</h1></body></html>\n",
        status, status_text(status), status, status_text(status));
    return send_simple(srv, fd, status, "text/html; charset=utf-8",
                       body, bn < 0 ? 0 : (size_t)bn, head_only, close_conn);
}

/* Stream a static file from docroot. */
static scl_error_t serve_static(const scl_http_server_t *srv, int fd,
                                const scl_http_request_t *req, bool head_only,
                                bool close_conn) {
    char resolved[SCL_PATH_MAX];
    int rc = resolve_in_docroot(srv, req->path, resolved, sizeof(resolved));
    if (rc != 0) return send_error(srv, fd, rc, head_only, close_conn);

    struct stat st;
    if (stat(resolved, &st) != 0)
        return send_error(srv, fd, 404, head_only, close_conn);

    /* Directory => try an index.html below it. */
    if (S_ISDIR(st.st_mode)) {
        char idx[SCL_PATH_MAX];
        char dpath[SCL_PATH_MAX];
        size_t pl = scl_strlen(req->path);
        const char *sep = (pl > 0 && req->path[pl - 1] == '/') ? "" : "/";
        int dn = snprintf(dpath, sizeof(dpath), "%s%sindex.html", req->path, sep);
        if (dn < 0 || (size_t)dn >= sizeof(dpath))
            return send_error(srv, fd, 404, head_only, close_conn);
        rc = resolve_in_docroot(srv, dpath, idx, sizeof(idx));
        if (rc != 0) return send_error(srv, fd, 403, head_only, close_conn);
        if (stat(idx, &st) != 0 || !S_ISREG(st.st_mode))
            return send_error(srv, fd, 404, head_only, close_conn);
        scl_memcpy(resolved, idx, scl_strlen(idx) + 1);
    } else if (!S_ISREG(st.st_mode)) {
        return send_error(srv, fd, 403, head_only, close_conn);
    }

    int ffd = open(resolved, O_RDONLY);
    if (ffd < 0) return send_error(srv, fd, 403, head_only, close_conn);

    char date[64];
    http_date(date, sizeof(date));
    char hdr[640];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: %s\r\n"
        "\r\n",
        date, srv->server_name, mime_for_path(resolved),
        (long long)st.st_size, close_conn ? "close" : "keep-alive");
    if (hn < 0 || (size_t)hn >= sizeof(hdr)) { close(ffd); return SCL_ERR_IO; }

    scl_error_t err = scl_tcp_send_all(fd, hdr, (size_t)hn);
    if (err == SCL_OK && !head_only) {
        char *buf = (char *)scl_alloc(srv->alloc, SCL_HTTP_SEND_CHUNK, _Alignof(max_align_t));
        if (!buf) { close(ffd); return SCL_ERR_OUT_OF_MEMORY; }
        for (;;) {
            ssize_t r = read(ffd, buf, SCL_HTTP_SEND_CHUNK);
            if (r > 0) { err = scl_tcp_send_all(fd, buf, (size_t)r); if (err != SCL_OK) break; }
            else if (r == 0) break;
            else if (errno == EINTR) continue;
            else { err = SCL_ERR_IO; break; }
        }
        scl_free(srv->alloc, buf);
    }
    close(ffd);
    return err;
}

/* ── Request parsing (operates in-place on the connection buffer) ─
 * Returns 0 on success, or an HTTP error status to send. */
static int parse_request(char *buf, size_t hdr_len, scl_http_request_t *req) {
    (void)scl_memset(req, 0, sizeof(*req));

    /* Split header block into CRLF-terminated lines (NUL-terminate each). */
    char *line = buf;
    char *block_end = buf + hdr_len;

    /* --- request line --- */
    char *eol = NULL;
    for (char *p = line; p + 1 < block_end; p++)
        if (p[0] == '\r' && p[1] == '\n') { eol = p; break; }
    if (!eol) return 400;
    *eol = '\0';
    char *next = eol + 2;

    /* METHOD SP TARGET SP HTTP/1.x */
    char *sp1 = scl_strchr(line, ' ');
    if (!sp1) return 400;
    *sp1 = '\0';
    char *target = sp1 + 1;
    char *sp2 = scl_strchr(target, ' ');
    if (!sp2) return 400;
    *sp2 = '\0';
    char *version = sp2 + 1;

    /* method: non-empty token of uppercase letters */
    if (line[0] == '\0') return 400;
    for (char *m = line; *m; m++)
        if (*m < 'A' || *m > 'Z') return 400;
    req->method = line;

    /* target: must begin with '/', bounded length, no control chars */
    size_t tlen = scl_strlen(target);
    if (tlen == 0 || target[0] != '/') return 400;
    if (tlen > SCL_HTTP_MAX_TARGET) return 414;
    for (size_t i = 0; i < tlen; i++)
        if ((unsigned char)target[i] < 0x20 || (unsigned char)target[i] == 0x7f)
            return 400;
    req->target = target;

    /* version */
    if (scl_strcmp(version, "HTTP/1.1") == 0)      req->version_minor = 1;
    else if (scl_strcmp(version, "HTTP/1.0") == 0) req->version_minor = 0;
    else return 505;
    req->keep_alive = (req->version_minor == 1);

    /* --- headers --- */
    while (next + 1 < block_end) {
        if (next[0] == '\r' && next[1] == '\n') break;   /* empty line: end */
        char *heol = NULL;
        for (char *p = next; p + 1 < block_end; p++)
            if (p[0] == '\r' && p[1] == '\n') { heol = p; break; }
        if (!heol) break;
        *heol = '\0';

        char *colon = scl_strchr(next, ':');
        if (colon) {
            *colon = '\0';
            char *val = colon + 1;
            while (*val == ' ' || *val == '\t') val++;
            if (req->header_count < SCL_HTTP_MAX_HEADERS) {
                req->headers[req->header_count].name  = next;
                req->headers[req->header_count].value = val;
                req->header_count++;
            }
        }
        next = heol + 2;
    }

    const char *conn_hdr = scl_http_request_header(req, "Connection");
    if (conn_hdr) {
        if (ci_contains_token(conn_hdr, "close"))      req->keep_alive = false;
        else if (ci_contains_token(conn_hdr, "keep-alive")) req->keep_alive = true;
    }
    return 0;
}

/* Locate end of header block ("\r\n\r\n"); returns offset past it or 0. */
static size_t find_header_end(const unsigned char *buf, size_t len) {
    if (len < 4) return 0;
    for (size_t i = 0; i + 3 < len; i++)
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
            return i + 4;
    return 0;
}

/* ── Per-connection service loop (keep-alive aware) ─────────── */
static void handle_connection(scl_http_server_t *srv, scl_tcp_conn_t *conn) {
    int fd = conn->fd;
    int served = 0;

    while (atomic_load_explicit(&srv->running, memory_order_relaxed) &&
           served < srv->cfg.keep_alive_max) {

        /* Read until we have a full header block (or hit a limit/timeout). */
        size_t hend = find_header_end(conn->rbuf, conn->rbuf_len);
        while (hend == 0) {
            if (conn->rbuf_len >= conn->rbuf_cap) {       /* headers too large */
                send_error(srv, fd, 431, false, true);
                return;
            }
            ssize_t n = recv(fd, conn->rbuf + conn->rbuf_len,
                             conn->rbuf_cap - conn->rbuf_len, 0);
            if (n > 0) {
                conn->rbuf_len += (size_t)n;
                hend = find_header_end(conn->rbuf, conn->rbuf_len);
            } else if (n == 0) {
                return;                                    /* peer closed */
            } else if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (served > 0) return;                    /* idle keep-alive: drop */
                send_error(srv, fd, 408, false, true);
                return;
            } else {
                return;
            }
        }

        scl_http_request_t req;
        int status = parse_request((char *)conn->rbuf, hend, &req);
        bool head_only = false;
        bool force_close = false;

        if (status != 0) {
            send_error(srv, fd, status, false, true);
            return;                                        /* malformed: close */
        }

        /* Reject request bodies and chunked framing: we serve no body methods,
         * and unconsumed bytes would desync keep-alive (smuggling). Any body
         * forces connection close after the response. */
        if (scl_http_request_header(&req, "Transfer-Encoding")) {
            send_error(srv, fd, 501, false, true);
            return;
        }
        const char *clen = scl_http_request_header(&req, "Content-Length");
        if (clen) {
            char *end = NULL;
            long long cl = scl_strtoll(clen, &end, 10);
            if (!end || *end != '\0' || cl < 0) {
                send_error(srv, fd, 400, false, true);
                return;
            }
            if (cl > 0) force_close = true;                /* don't reframe */
        }

        bool is_get  = scl_strcmp(req.method, "GET") == 0;
        bool is_head = scl_strcmp(req.method, "HEAD") == 0;
        head_only = is_head;

        bool will_close = force_close || !req.keep_alive ||
                          (served + 1 >= srv->cfg.keep_alive_max);

        /* Decode path into a stack buffer; point req.path at it. */
        char pathbuf[SCL_PATH_MAX];
        const char *q = scl_strchr(req.target, '?');
        size_t rawlen = q ? (size_t)(q - req.target) : scl_strlen(req.target);
        long dl = url_decode_path(req.target, rawlen, pathbuf, sizeof(pathbuf));
        if (dl < 0) {
            send_error(srv, fd, 400, head_only, true);
            return;                                        /* malformed target: close */
        }
        req.path = pathbuf;

        scl_error_t serr = SCL_OK;
        scl_http_response_t resp = {0};
        if (srv->cfg.handler && srv->cfg.handler(&req, &resp, srv->cfg.handler_user)) {
            if (resp.close) will_close = true;
            serr = send_simple(srv, fd, resp.status ? resp.status : 200,
                               resp.content_type, resp.body, resp.body_len,
                               head_only, will_close);
        } else if (is_get || is_head) {
            serr = serve_static(srv, fd, &req, head_only, will_close);
        } else {
            serr = send_error(srv, fd, 405, head_only, will_close);
            will_close = true;
        }

        if (serr != SCL_OK || will_close) return;

        /* Preserve any pipelined bytes that follow this request. */
        if (conn->rbuf_len > hend)
            scl_memmove(conn->rbuf, conn->rbuf + hend, conn->rbuf_len - hend);
        conn->rbuf_len -= hend;
        served++;
    }
}

/* ── Worker / acceptor threads ──────────────────────────────── */
static bool sem_wait_or_stop(scl_http_server_t *srv) {
    scl_mutex_lock(&srv->sem_m);
    while (srv->sem_count == 0 &&
           atomic_load_explicit(&srv->running, memory_order_relaxed))
        scl_cond_wait(&srv->sem_c, &srv->sem_m);
    bool have = srv->sem_count > 0;
    if (have) srv->sem_count--;
    scl_mutex_unlock(&srv->sem_m);
    return have;
}

static void sem_post(scl_http_server_t *srv) {
    scl_mutex_lock(&srv->sem_m);
    srv->sem_count++;
    scl_cond_signal(&srv->sem_c);
    scl_mutex_unlock(&srv->sem_m);
}

static void *worker_main(void *arg) {
    scl_http_server_t *srv = (scl_http_server_t *)arg;
    while (atomic_load_explicit(&srv->running, memory_order_relaxed)) {
        if (!sem_wait_or_stop(srv)) break;
        scl_tcp_conn_t *conn = scl_tcp_pool_get_ready(&srv->pool);
        if (!conn) continue;
        handle_connection(srv, conn);
        scl_tcp_pool_release(&srv->pool, conn);
    }
    return NULL;
}

static void *acceptor_main(void *arg) {
    scl_http_server_t *srv = (scl_http_server_t *)arg;
    struct pollfd pfd = { .fd = srv->listen_fd, .events = POLLIN, .revents = 0 };

    while (atomic_load_explicit(&srv->running, memory_order_relaxed)) {
        int pr = poll(&pfd, 1, 200);
        if (pr <= 0) continue;                 /* timeout / EINTR: recheck running */

        for (;;) {                             /* drain the accept backlog */
            scl_tcp_conn_t tmp;
            tmp.fd = -1;
            scl_error_t aerr = scl_tcp_accept(srv->listen_fd, &tmp);
            if (aerr == SCL_ERR_TIMEOUT) break;    /* would-block: done for now */
            if (aerr != SCL_OK) break;

            scl_tcp_conn_t *conn = scl_tcp_pool_acquire(&srv->pool);
            if (!conn) { close(tmp.fd); continue; }   /* at capacity: drop */

            conn->fd       = tmp.fd;
            conn->peer     = tmp.peer;
            conn->peer_len = tmp.peer_len;
            conn->rbuf_len = 0;
            scl_tcp_set_recv_timeout(conn->fd, srv->cfg.recv_timeout_ms);

            if (!scl_tcp_pool_post_ready(&srv->pool, conn)) {
                scl_tcp_pool_release(&srv->pool, conn);   /* queue full: drop */
                continue;
            }
            sem_post(srv);
        }
    }
    return NULL;
}

/* ── Lifecycle ──────────────────────────────────────────────── */
static void apply_defaults(scl_http_config_t *c) {
    if (c->num_workers   <= 0) c->num_workers   = 4;
    if (c->pool_capacity == 0) c->pool_capacity = 256;
    if (c->conn_buf_cap  == 0) c->conn_buf_cap  = 64 * 1024;
    if (c->recv_timeout_ms == 0) c->recv_timeout_ms = 15000;
    if (c->keep_alive_max <= 0) c->keep_alive_max = 100;
    if (c->backlog       <= 0) c->backlog       = 128;
    if (!c->server_name)       c->server_name   = "scl-httpd";
}

scl_error_t scl_http_server_init(scl_allocator_t *alloc, scl_http_server_t **out,
                                 const scl_http_config_t *cfg) {
    if (scl_unlikely(!alloc || !out || !cfg)) return SCL_ERR_NULL_PTR;
    *out = NULL;

    scl_net_init();   /* ignore SIGPIPE */

    scl_http_server_t *srv = (scl_http_server_t *)scl_calloc(alloc, 1,
                                sizeof(scl_http_server_t), _Alignof(scl_http_server_t));
    if (!srv) return SCL_ERR_OUT_OF_MEMORY;
    srv->alloc = alloc;
    srv->cfg   = *cfg;
    apply_defaults(&srv->cfg);
    srv->num_workers = srv->cfg.num_workers;

    scl_strlcpy(srv->server_name, srv->cfg.server_name, sizeof(srv->server_name));

    if (srv->cfg.docroot) {
        if (!realpath(srv->cfg.docroot, srv->docroot_real)) {
            scl_free(alloc, srv);
            return SCL_ERR_NOT_FOUND;
        }
        srv->docroot_real_len = scl_strlen(srv->docroot_real);
    }

    scl_error_t err = scl_tcp_pool_init(alloc, &srv->pool,
                                        srv->cfg.pool_capacity, srv->cfg.conn_buf_cap);
    if (err != SCL_OK) { scl_free(alloc, srv); return err; }

    err = scl_tcp_listen(srv->cfg.host, srv->cfg.port, srv->cfg.backlog, &srv->listen_fd);
    if (err != SCL_OK) {
        scl_tcp_pool_destroy(&srv->pool);
        scl_free(alloc, srv);
        return err;
    }

    /* Resolve the actual bound port (supports port 0). */
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    if (getsockname(srv->listen_fd, (struct sockaddr *)&ss, &slen) == 0) {
        if (ss.ss_family == AF_INET)
            srv->bound_port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
        else if (ss.ss_family == AF_INET6)
            srv->bound_port = ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
    }

    scl_tcp_set_nonblocking(srv->listen_fd, true);

    scl_mutex_init(&srv->sem_m);
    scl_cond_init(&srv->sem_c);
    srv->sem_count = 0;
    atomic_init(&srv->running, false);
    atomic_init(&srv->started, false);

    srv->workers = (scl_thread_t *)scl_calloc(alloc, (size_t)srv->num_workers,
                                              sizeof(scl_thread_t), _Alignof(scl_thread_t));
    if (!srv->workers) {
        close(srv->listen_fd);
        scl_mutex_destroy(&srv->sem_m);
        scl_cond_destroy(&srv->sem_c);
        scl_tcp_pool_destroy(&srv->pool);
        scl_free(alloc, srv);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    *out = srv;
    return SCL_OK;
}

scl_error_t scl_http_server_start(scl_http_server_t *srv) {
    if (scl_unlikely(!srv)) return SCL_ERR_NULL_PTR;
    if (atomic_exchange(&srv->started, true)) return SCL_ERR_INVALID_STATE;

    atomic_store(&srv->running, true);

    for (int i = 0; i < srv->num_workers; i++) {
        if (scl_thread_create(&srv->workers[i], worker_main, srv) != SCL_OK) {
            atomic_store(&srv->running, false);
            /* wake any workers already parked, then join the ones we spawned */
            scl_mutex_lock(&srv->sem_m);
            scl_cond_broadcast(&srv->sem_c);
            scl_mutex_unlock(&srv->sem_m);
            for (int j = 0; j < i; j++) scl_thread_join(srv->workers[j], NULL);
            return SCL_ERR_ALLOC;
        }
    }

    if (scl_thread_create(&srv->acceptor, acceptor_main, srv) != SCL_OK) {
        scl_http_server_stop(srv);
        return SCL_ERR_ALLOC;
    }
    return SCL_OK;
}

scl_error_t scl_http_server_stop(scl_http_server_t *srv) {
    if (scl_unlikely(!srv)) return SCL_ERR_NULL_PTR;
    if (!atomic_exchange(&srv->running, false)) {
        if (!atomic_load(&srv->started)) return SCL_OK;
    }

    /* Wake every parked worker so they observe !running. */
    scl_mutex_lock(&srv->sem_m);
    scl_cond_broadcast(&srv->sem_c);
    scl_mutex_unlock(&srv->sem_m);

    if (atomic_load(&srv->started)) {
        scl_thread_join(srv->acceptor, NULL);
        for (int i = 0; i < srv->num_workers; i++)
            scl_thread_join(srv->workers[i], NULL);
        atomic_store(&srv->started, false);
    }
    return SCL_OK;
}

uint16_t scl_http_server_port(const scl_http_server_t *srv) {
    return srv ? srv->bound_port : 0;
}

void scl_http_server_destroy(scl_http_server_t *srv) {
    if (!srv) return;
    scl_http_server_stop(srv);
    if (srv->listen_fd >= 0) close(srv->listen_fd);
    scl_mutex_destroy(&srv->sem_m);
    scl_cond_destroy(&srv->sem_c);
    scl_tcp_pool_destroy(&srv->pool);
    scl_free(srv->alloc, srv->workers);
    scl_free(srv->alloc, srv);
}
