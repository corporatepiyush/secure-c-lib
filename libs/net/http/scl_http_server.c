/*
 * scl_http_server.c — HTTP/1.1 server implementation.
 *
 * ── What this file does ─────────────────────────────────────────────────────
 *
 * This is the companion server to scl_http_client.c. It:
 *
 *   1. Binds a TCP port and listens for connections.
 *   2. Accepts connections into a lock-free pool of pre-allocated slots.
 *   3. Dispatches connections to worker threads via an MPMC queue.
 *   4. Parses HTTP/1.1 request lines and headers (in-place, on the connection's
 *      read buffer — no extra per-request malloc).
 *   5. Reads request bodies (Content-Length and chunked transfer).
 *   6. Parses multipart/form-data for file uploads.
 *   7. Responds with static files (from a docroot) or dynamic handler output.
 *   8. Supports keep-alive, Content-Length, path-traversal defence, and
 *      percent-decoding.
 *
 * ── Security design (first priority) ────────────────────────────────────────
 *
 * HTTP servers are exposed to untrusted input on every request. The following
 * attacks are specifically defended against:
 *
 *   • Request smuggling: If Transfer-Encoding is present, Content-Length is
 *     ignored (per RFC 7230 §3.3.1). We read the body exactly once using the
 *     chosen framing — no ambiguity.
 *   • Path traversal: Percent-decode the path, reject ".." segments before
 *     touching the filesystem, then validate via realpath() containment.
 *   • Encoded NUL (%00): Explicitly rejected in url_decode_path().
 *   • Buffer overflow: All parsing operates on bounded buffers. Header count
 *     and target length are capped; per-connection read buffer is bounded.
 *   • Response splitting: No user-controlled data appears in format strings.
 *   • Information disclosure: Connection buffers are zeroed on release.
 */

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
#include <ctype.h>

#define SCL_HTTP_MAX_TARGET  8190
#define SCL_HTTP_SEND_CHUNK  (64 * 1024)
#define SCL_HTTP_MAX_BODY_DEFAULT (1024 * 1024) /* 1MB */

/*
 * ── Server object ──────────────────────────────────────────────────────
 *
 * The server struct is intentionally opaque (typedef'd in the header).
 * Internal state includes:
 *   • alloc — the allocator used for worker array and internal allocations
 *   • cfg — snapshot of the user's config (defaults applied at init)
 *   • server_name — copied into a fixed buffer (no dangling pointer risk)
 *   • docroot_real — realpath() resolved docroot (used for containment check)
 *   • listen_fd / bound_port — bound socket and resolved port
 *   • pool — the lock-free connection pool
 *   • threads — acceptor thread + worker threads
 *   • sem_* — condvar-based semaphore for parking idle workers
 *   • running / started — atomic flags for lifecycle control
 */
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

    atomic_bool        running SCL_CACHE_ALIGNED;
    atomic_bool        started SCL_CACHE_ALIGNED;
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

/*
 * ── Status line helpers ───────────────────────────────────────────
 *
 * These are purely internal helpers for constructing well-formed HTTP
 * responses. They don't deal with user data, so the security concerns
 * are about correctness (valid HTTP, consistent headers).
 */

/* Map integer code to standard reason phrase per RFC 7231 §6. */
static const char *status_text(int code) {
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
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
/* Compare up to prefix_len bytes of 'full' against 'expected' (case-insensitive) */
static int ci_prefix(const char *full, size_t prefix_len, const char *expected) {
    for (size_t i = 0; i < prefix_len; i++) {
        if (expected[i] == '\0')
            return 0;
        if (scl_tolower((unsigned char)full[i]) != scl_tolower((unsigned char)expected[i]))
            return 0;
    }
    return expected[prefix_len] == '\0';
}

static int ci_equal(const char *a, const char *b) {
    while (scl_likely(*a && *b)) {
        if (scl_unlikely(scl_tolower((unsigned char)*a) != scl_tolower((unsigned char)*b)))
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
    if (!haystack) return 0;
    size_t tl = scl_strlen(token);
    for (const char *p = haystack; *p; p++) {
        size_t i = 0;
        while (i < tl && p[i] &&
               scl_tolower((unsigned char)p[i]) == scl_tolower((unsigned char)token[i]))
            i++;
        if (i == tl) {
            char c = p[i];
            if (c == '\0' || c == ' ' || c == ',' || c == '\t' || c == '\r' || c == ';')
                return 1;
        }
    }
    return 0;
}

/* ── Date header ────────────────────────────────────────────── */
static void http_date(char *out, size_t cap) {
    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    strftime(out, cap, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
}

/*
 * ── Percent-decoding ────────────────────────────────────────────────────
 */
static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

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
            if (v == 0) return -1;
            c = (char)v;
            i += 2;
        }
        if (c == '\0') return -1;
        if (o + 1 >= dstcap) return -1;
        dst[o++] = c;
    }
    dst[o] = '\0';
    return (long)o;
}

/*
 * ── Path-traversal-safe resolution within docroot ────────────────────
 */
static int resolve_in_docroot(const scl_http_server_t *srv, const char *decoded_path,
                              char *resolved, size_t cap) {
    if (srv->docroot_real_len == 0) return 404;
    if (decoded_path[0] != '/') return 400;

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
    if (!realpath(candidate, real)) return 404;

    size_t rl = scl_strlen(real);
    if (rl < srv->docroot_real_len) return 403;
    if (scl_memcmp(real, srv->docroot_real, srv->docroot_real_len) != 0) return 403;
    if (rl > srv->docroot_real_len && real[srv->docroot_real_len] != '/') return 403;

    if (rl + 1 > cap) return 414;
    scl_memcpy(resolved, real, rl + 1);
    return 0;
}

/*
 * ── Response helpers ──────────────────────────────────────────────
 */
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

static scl_error_t serve_static(const scl_http_server_t *srv, int fd,
                                const scl_http_request_t *req, bool head_only,
                                bool close_conn) {
    char resolved[SCL_PATH_MAX];
    int rc = resolve_in_docroot(srv, req->path, resolved, sizeof(resolved));
    if (rc != 0) return send_error(srv, fd, rc, head_only, close_conn);

    struct stat st;
    if (stat(resolved, &st) != 0)
        return send_error(srv, fd, 404, head_only, close_conn);

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

/*
 * ── Chunked transfer decoding ───────────────────────────────────────
 *
 * Reads and decodes a chunked transfer body from fd. Returns the
 * reassembled body in *out (allocated via alloc) and its length in
 * *out_len. On error, returns an HTTP status code (0 on success).
 */
static int read_chunked_body(scl_allocator_t *alloc, int fd,
                             unsigned char *SCL_RESTRICT rbuf,
                             size_t *SCL_RESTRICT rbuf_len, size_t rbuf_cap,
                             size_t head_end, size_t max_body,
                             void *SCL_RESTRICT *SCL_RESTRICT out,
                             size_t *SCL_RESTRICT out_len) {
    size_t cap = 4096;
    unsigned char *buf = (unsigned char *)scl_alloc(alloc, cap, _Alignof(max_align_t));
    if (!buf) return 500;
    size_t len = 0;

    /* Consume any data already in rbuf after the header block */
    size_t offset = head_end;

    /* Read until we have a full line (chunk-size) */
    while (1) {
        size_t i;
        for (i = offset; i + 1 < *rbuf_len; i++)
            if (rbuf[i] == '\r' && rbuf[i+1] == '\n') break;

        if (i + 1 < *rbuf_len) {
            /* Found CRLF — parse chunk-size line */
            rbuf[i] = '\0';
            char *end = NULL;
            long long chunk_sz = scl_strtoll((const char *)rbuf + offset, &end, 16);
            if (end == (char *)rbuf + offset) { scl_free(alloc, buf); return 400; }
            if (chunk_sz < 0) { scl_free(alloc, buf); return 400; }

            offset = i + 2; /* skip past CRLF */

            if (chunk_sz == 0) {
                /* Last chunk — read trailer headers until blank line */
                while (1) {
                    /* Need data */
                    while (offset + 1 >= *rbuf_len) {
                        ssize_t n = recv(fd, rbuf + *rbuf_len,
                                         rbuf_cap - *rbuf_len, 0);
                        if (n > 0) { *rbuf_len += (size_t)n; }
                        else if (n == 0) { scl_free(alloc, buf); return 400; }
                        else if (errno == EINTR) continue;
                        else { scl_free(alloc, buf); return 500; }
                    }
                    size_t j;
                    for (j = offset; j + 1 < *rbuf_len; j++)
                        if (rbuf[j] == '\r' && rbuf[j+1] == '\n') break;
                    if (j + 1 < *rbuf_len) {
                        if (j == offset) {
                            /* Blank line — end of trailers.
                             * Compact trailing CRLF, keep pipelined bytes. */
                            offset += 2;
                            if (offset < *rbuf_len) {
                                memmove(rbuf, rbuf + offset, *rbuf_len - offset);
                                *rbuf_len -= offset;
                            } else {
                                *rbuf_len = 0;
                            }
                            *out = buf;
                            *out_len = len;
                            return 0;
                        }
                        offset = j + 2;
                    } else {
                        /* Need more data — rbuf may not have full trailer line */
                        break;
                    }
                }
                /* If we break out without finding blank line, we'll loop back
                 * and read more. This is fine — the blank line will follow. */
                continue;
            }

            /* Read chunk data */
            size_t remaining = (size_t)chunk_sz;
            while (remaining > 0) {
                size_t avail = *rbuf_len - offset;
                size_t to_copy = avail < remaining ? avail : remaining;
                if (to_copy > 0) {
                    if (len + to_copy > max_body) { scl_free(alloc, buf); return 413; }
                    if (len + to_copy > cap) {
                        size_t new_cap = cap * 2;
                        while (new_cap < len + to_copy) new_cap *= 2;
                        unsigned char *nb = (unsigned char *)scl_realloc(alloc, buf, cap, new_cap, _Alignof(max_align_t));
                        if (!nb) { scl_free(alloc, buf); return 500; }
                        buf = nb;
                        cap = new_cap;
                    }
                    scl_memcpy(buf + len, rbuf + offset, to_copy);
                    len += to_copy;
                    offset += to_copy;
                    remaining -= to_copy;
                }
                if (remaining > 0) {
                    ssize_t n = recv(fd, rbuf, rbuf_cap, 0);
                    if (n > 0) {
                        *rbuf_len = (size_t)n;
                        offset = 0;
                    } else if (n == 0) { scl_free(alloc, buf); return 400; }
                    else if (errno == EINTR) continue;
                    else { scl_free(alloc, buf); return 500; }
                }
            }

            /* Consume trailing CRLF */
            while (offset + 1 >= *rbuf_len) {
                ssize_t n = recv(fd, rbuf + *rbuf_len,
                                 rbuf_cap - *rbuf_len, 0);
                if (n > 0) { *rbuf_len += (size_t)n; }
                else if (n == 0) { scl_free(alloc, buf); return 400; }
                else if (errno == EINTR) continue;
                else { scl_free(alloc, buf); return 500; }
            }
            if (rbuf[offset] == '\r' && rbuf[offset+1] == '\n') {
                offset += 2;
            } else {
                scl_free(alloc, buf);
                return 400;
            }

            /* Compact rbuf */
            if (offset < *rbuf_len) {
                memmove(rbuf, rbuf + offset, *rbuf_len - offset);
                *rbuf_len -= offset;
            } else {
                *rbuf_len = 0;
            }
            offset = 0;
        } else {
            /* Need more data for chunk-size line */
            ssize_t n = recv(fd, rbuf + *rbuf_len, rbuf_cap - *rbuf_len, 0);
            if (n > 0) { *rbuf_len += (size_t)n; }
            else if (n == 0) { scl_free(alloc, buf); return 400; }
            else if (errno == EINTR) continue;
            else { scl_free(alloc, buf); return 500; }
        }
    }
}

/*
 * ── Multipart/form-data parser ─────────────────────────────────────
 *
 * Parse a multipart/form-data body (RFC 2046, RFC 7578). The body buffer
 * is modified in-place (NUL-terminated line endings) and the upload_t
 * entries point into it. No copying of part data.
 */
int scl_http_parse_multipart(const void *body, size_t body_len,
                             const char *content_type,
                             scl_http_upload_t *uploads, size_t *count,
                             size_t max_count) {
    if (!body || !content_type || !count || !uploads) return -1;

    /* Extract boundary from Content-Type */
    const char *boundary_str = scl_strstr(content_type, "boundary=");
    if (!boundary_str) return -1;
    boundary_str += 9; /* skip "boundary=" */
    /* Trim possible quotes */
    char boundary[256];
    size_t bi = 0;
    if (*boundary_str == '"') boundary_str++;
    while (*boundary_str && *boundary_str != ';' && *boundary_str != '"' && *boundary_str != ' ') {
        if (bi >= sizeof(boundary) - 1) return -1;
        boundary[bi++] = *boundary_str++;
    }
    boundary[bi] = '\0';
    if (bi == 0) return -1;

    /* Delimiter markers */
    char delim[512];
    char end_delim[512];
    snprintf(delim, sizeof(delim), "--%s", boundary);
    snprintf(end_delim, sizeof(end_delim), "--%s--", boundary);
    size_t dlen = scl_strlen(delim);

    unsigned char *ptr = (unsigned char *)body;
    size_t remaining = body_len;
    size_t parsed = 0;

    while (remaining > 0 && *count < max_count) {
        /* Skip leading whitespace / CRLF before delimiter */
        while (remaining > 0 && (*ptr == '\r' || *ptr == '\n')) {
            ptr++; remaining--;
        }
        if (remaining < dlen) break;

        /* Check if this is the end delimiter */
        if (scl_memcmp(ptr, end_delim, scl_strlen(end_delim)) == 0) break;
        if (scl_memcmp(ptr, delim, dlen) != 0) break;

        ptr += dlen;
        remaining -= dlen;

        /* Skip CRLF after delimiter */
        if (remaining >= 2 && ptr[0] == '\r' && ptr[1] == '\n') {
            ptr += 2; remaining -= 2;
        } else if (remaining >= 1 && ptr[0] == '\n') {
            ptr += 1; remaining -= 1;
        }

        /* Parse part headers until blank line */
        const char *part_content_type = NULL;
        const char *name = NULL;
        const char *filename = NULL;

        while (remaining > 0) {
            /* Find end of this header line */
            size_t hdr_end = 0;
            bool found = false;
            for (size_t i = 0; i + 1 < remaining; i++) {
                if (ptr[i] == '\r' && ptr[i+1] == '\n') {
                    hdr_end = i;
                    found = true;
                    break;
                }
            }
            if (!found) break;

            if (hdr_end == 0) {
                /* Blank line — end of headers */
                ptr += 2; remaining -= 2; /* skip CRLF */
                break;
            }

            /* Locate colon separating header name from value */
            size_t colon_pos = 0;
            for (size_t i = 0; i < hdr_end; i++) {
                if (ptr[i] == ':') { colon_pos = i; break; }
            }
            if (colon_pos == 0 || colon_pos >= hdr_end) goto next_hdr;

            const unsigned char *hdr_value_p = ptr + colon_pos + 1;
            while ((size_t)(hdr_value_p - ptr) < hdr_end && *hdr_value_p == ' ')
                hdr_value_p++;

            /* Compare header name case-insensitively by scanning to colon */
#define CI_HDR_EQ(expected) ci_prefix((const char *)ptr, colon_pos, expected)
            /* Content-Disposition: form-data; name="field"; filename="file.txt" */
            if (CI_HDR_EQ("Content-Disposition")) {
                for (const char *p = (const char *)hdr_value_p; p < (const char *)ptr + hdr_end; p++) {
                    if ((size_t)((const char *)ptr + hdr_end - p) >= 6 &&
                        scl_memcmp(p, "name=\"", 6) == 0) {
                        name = p + 6;
                    }
                    if ((size_t)((const char *)ptr + hdr_end - p) >= 10 &&
                        scl_memcmp(p, "filename=\"", 10) == 0) {
                        filename = p + 10;
                    }
                }
            }

            if (CI_HDR_EQ("Content-Type")) {
                part_content_type = (const char *)hdr_value_p;
            }
#undef CI_HDR_EQ

next_hdr:
            ptr += hdr_end + 2; /* skip CRLF */
            remaining -= (hdr_end + 2);
        }

        /* Remaining bytes until next delimiter are part body */
        /* Find next "--boundary" or "--boundary--" */
        size_t part_end = remaining;
        for (size_t i = 0; i + dlen < remaining; i++) {
            if (ptr[i] == '\r' && ptr[i+1] == '\n' &&
                (size_t)i + 2 + dlen <= remaining &&
                scl_memcmp(ptr + i + 2, delim, dlen) == 0) {
                part_end = (size_t)i;
                break;
            }
            if (ptr[i] == '\r' && ptr[i+1] == '\n' &&
                (size_t)i + 2 + dlen + 2 <= remaining &&
                scl_memcmp(ptr + i + 2, end_delim, scl_strlen(end_delim)) == 0) {
                part_end = (size_t)i;
                break;
            }
        }

        if (part_end > remaining) part_end = remaining;

        if (*count < max_count) {
            uploads[*count].name = name;
            uploads[*count].filename = filename;
            uploads[*count].content_type = part_content_type;
            uploads[*count].data = ptr;
            uploads[*count].data_len = part_end;
            (*count)++;
        }

        ptr += part_end;
        remaining -= part_end;
        parsed++;
    }

    return (int)parsed;
}

/*
 * ── Request parsing ─────────────────────────────────────────────────
 *
 * Parses an HTTP/1.x request from a NUL-terminated buffer by replacing
 * CRLF line endings with C NULs and pointing the request struct's fields
 * at the right offsets within the original buffer. This is ZERO-COPY:
 * we don't allocate or copy any part of the request — we just NUL-
 * terminate each piece in place.
 */
static int parse_request(char *buf, size_t hdr_len, scl_http_request_t *req) {
    (void)scl_memset(req, 0, sizeof(*req));

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
    if (scl_unlikely(!sp1)) return 400;
    *sp1 = '\0';
    char *target = sp1 + 1;
    char *sp2 = scl_strchr(target, ' ');
    if (scl_unlikely(!sp2)) return 400;
    *sp2 = '\0';
    char *version = sp2 + 1;

    if (line[0] == '\0') return 400;
    for (char *m = line; *m; m++)
        if (*m < 'A' || *m > 'Z') return 400;
    req->method = line;

    size_t tlen = scl_strlen(target);
    if (tlen == 0 || target[0] != '/') return 400;
    if (tlen > SCL_HTTP_MAX_TARGET) return 414;
    for (size_t i = 0; i < tlen; i++)
        if ((unsigned char)target[i] < 0x20 || (unsigned char)target[i] == 0x7f)
            return 400;
    req->target = target;

    if (scl_strcmp(version, "HTTP/1.1") == 0)      req->version_minor = 1;
    else if (scl_strcmp(version, "HTTP/1.0") == 0) req->version_minor = 0;
    else return 505;
    req->keep_alive = (req->version_minor == 1);

    /* --- headers --- */
    while (next + 1 < block_end) {
        if (scl_unlikely(next[0] == '\r' && next[1] == '\n')) break;
        char *heol = NULL;
        for (char *p = next; p + 1 < block_end; p++)
            if (scl_likely(p[0] == '\r' && p[1] == '\n')) { heol = p; break; }
        if (scl_unlikely(!heol)) break;
        *heol = '\0';

        char *colon = scl_strchr(next, ':');
        if (scl_likely(colon != NULL)) {
            *colon = '\0';
            char *val = colon + 1;
            while (*val == ' ' || *val == '\t') val++;
            if (scl_likely(req->header_count < SCL_HTTP_MAX_HEADERS)) {
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

static size_t find_header_end(const unsigned char *buf, size_t len) {
    if (scl_unlikely(len < 4)) return 0;
    for (size_t i = 0; i + 3 < len; i++)
        if (scl_likely(buf[i] == '\r' && buf[i+1] == '\n' &&
                       buf[i+2] == '\r' && buf[i+3] == '\n'))
            return i + 4;
    return 0;
}

/*
 * ── Body reading for Content-Length ─────────────────────────────────
 *
 * Reads exactly `cl` bytes from the connection. Uses any bytes already
 * in rbuf (after the header block). Returns 0 on success or HTTP error.
 */
static int read_body_fixed(scl_allocator_t *alloc, int fd,
                           unsigned char *SCL_RESTRICT rbuf,
                           size_t *SCL_RESTRICT rbuf_len, size_t hend,
                           long long cl, size_t max_body,
                           void *SCL_RESTRICT *SCL_RESTRICT out,
                           size_t *SCL_RESTRICT out_len) {
    if (cl == 0) { *out = NULL; *out_len = 0; return 0; }
    if ((unsigned long long)cl > max_body) return 413;

    size_t need = (size_t)cl;
    unsigned char *buf = (unsigned char *)scl_alloc(alloc, need, _Alignof(max_align_t));
    if (!buf) return 500;
    size_t got = 0;

    /* Copy data already in rbuf after header end */
    size_t avail = *rbuf_len > hend ? *rbuf_len - hend : 0;
    if (avail > 0) {
        size_t copy = avail < need ? avail : need;
        scl_memcpy(buf, rbuf + hend, copy);
        got = copy;
    }

    while (got < need) {
        ssize_t n = recv(fd, buf + got, need - got, 0);
        if (scl_likely(n > 0)) {
            got += (size_t)n;
        } else if (scl_unlikely(n == 0)) {
            scl_free(alloc, buf);
            return 400;
        } else if (errno == EINTR) {
            continue;
        } else {
            scl_free(alloc, buf);
            return 500;
        }
    }

    /* Compact rbuf: body bytes consumed, keep any remainder after body */
    if (hend + need < *rbuf_len) {
        size_t rest = *rbuf_len - (hend + need);
        memmove(rbuf, rbuf + hend + need, rest);
        *rbuf_len = rest;
    } else {
        *rbuf_len = 0;
    }

    *out = buf;
    *out_len = need;
    return 0;
}

/*
 * ── Connection handler (keep-alive loop) ──────────────────────────
 *
 * This is the main per-connection service routine, called by a worker
 * thread after it dequeues a connection from the ready queue.
 *
 * The loop:
 *   1. Accumulate data until we have a complete header block ("\r\n\r\n").
 *   2. Parse the request line and headers (in-place, on the rbuf).
 *   3. Validate Host header (required for HTTP/1.1).
 *   4. Determine framing (CL vs chunked), read the request body.
 *   5. Decode the URL path (percent-decode) into a stack buffer.
 *   6. Parse multipart/form-data if content type matches.
 *   7. Dispatch to the dynamic handler (if configured) or static serving.
 *   8. On keep-alive, compact the rbuf to preserve pipelined bytes and
 *      continue the outer while loop.
 */
static void handle_connection(scl_http_server_t *srv, scl_tcp_conn_t *conn) {
    int fd = conn->fd;
    int served = 0;
    scl_allocator_t *alloc = srv->alloc;
    size_t max_body = srv->cfg.max_body_size > 0 ? srv->cfg.max_body_size : SCL_HTTP_MAX_BODY_DEFAULT;

    while (atomic_load_explicit(&srv->running, memory_order_acquire) &&
           served < srv->cfg.keep_alive_max) {

        /* Read until we have a full header block (or hit a limit/timeout). */
        size_t hend = find_header_end(conn->rbuf, conn->rbuf_len);
        while (scl_unlikely(hend == 0)) {
            if (scl_unlikely(conn->rbuf_len >= conn->rbuf_cap)) {
                send_error(srv, fd, 431, false, true);
                return;
            }
            ssize_t n = recv(fd, conn->rbuf + conn->rbuf_len,
                             conn->rbuf_cap - conn->rbuf_len, 0);
            if (scl_likely(n > 0)) {
                conn->rbuf_len += (size_t)n;
                hend = find_header_end(conn->rbuf, conn->rbuf_len);
            } else if (scl_unlikely(n == 0)) {
                return;
            } else if (errno == EINTR) {
                continue;
            } else if (scl_unlikely(errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (served > 0) return;
                send_error(srv, fd, 408, false, true);
                return;
            } else {
                return;
            }
        }

        scl_http_request_t req;
        int status = parse_request((char *)conn->rbuf, hend, &req);
        bool head_only = false;
        bool will_close = false;
        bool body_allocated = false;

        if (status != 0) {
            send_error(srv, fd, status, false, true);
            return;
        }

        /* HTTP/1.1 requires Host header (RFC 7230 §5.4) */
        if (req.version_minor == 1) {
            const char *host = scl_http_request_header(&req, "Host");
            if (!host || host[0] == '\0') {
                send_error(srv, fd, 400, false, true);
                return;
            }
        }

        /* Store Content-Type for handler convenience */
        req.content_type = scl_http_request_header(&req, "Content-Type");

        /* ── Determine request body framing ──────────────────── */
        bool is_chunked = false;
        long long content_length = 0;
        bool valid_cl = false;

        /* Collect all Transfer-Encoding headers, concatenate (RFC 7230 §3.2.2) */
        char te_buf[256];
        size_t te_len = 0;
        te_buf[0] = '\0';
        for (size_t _i = 0; _i < req.header_count; _i++) {
            if (ci_equal(req.headers[_i].name, "Transfer-Encoding")) {
                size_t vlen = scl_strlen(req.headers[_i].value);
                if (vlen >= sizeof(te_buf)) {
                    send_error(srv, fd, 431, false, true); return;
                }
                if (te_len > 0) {
                    if (te_len + 2 + vlen >= sizeof(te_buf)) {
                        send_error(srv, fd, 431, false, true); return;
                    }
                    te_buf[te_len++] = ','; te_buf[te_len++] = ' ';
                }
                scl_memcpy(te_buf + te_len, req.headers[_i].value, vlen);
                te_len += vlen;
            }
        }
        te_buf[te_len] = '\0';

        /* Validate Content-Length: reject duplicates with differing values (RFC 7230 §3.3.2) */
        int cl_count = 0;
        long long first_cl = -1;
        for (size_t _i = 0; _i < req.header_count; _i++) {
            if (ci_equal(req.headers[_i].name, "Content-Length")) {
                char *end = NULL;
                long long val = scl_strtoll(req.headers[_i].value, &end, 10);
                if (!end || *end != '\0' || val < 0) {
                    send_error(srv, fd, 400, false, true); return;
                }
                if (cl_count == 0) first_cl = val;
                else if (val != first_cl) {
                    send_error(srv, fd, 400, false, true); return;
                }
                cl_count++;
            }
        }

        const char *te = te_len > 0 ? te_buf : NULL;

        if (te) {
            /* Transfer-Encoding takes precedence (RFC 7230 §3.3.1).
             * Find the last encoding token (after the final comma). */
            const char *last_te = te_buf;
            for (const char *_p = te_buf; _p < te_buf + te_len; _p++)
                if (*_p == ',') last_te = _p + 1;
            while (*last_te == ' ' || *last_te == '\t') last_te++;

            if (ci_contains_token(last_te, "chunked")) {
                is_chunked = true;
            } else if (ci_contains_token(te_buf, "chunked")) {
                /* chunked present but not the final encoding — read until close */
                send_error(srv, fd, 501, false, true);
                return;
            } else {
                send_error(srv, fd, 501, false, true);
                return;
            }
        }

        if (!is_chunked && cl_count > 0) {
            content_length = first_cl;
            valid_cl = true;
        }

        /* Handle Expect: 100-continue (RFC 7231 §5.1.1) */
        const char *expect = scl_http_request_header(&req, "Expect");
        if (expect) {
            if (ci_contains_token(expect, "100-continue")) {
                static const char _cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
                scl_tcp_send_all(fd, _cont, sizeof(_cont) - 1);
            } else {
                send_error(srv, fd, 417, false, true);
                return;
            }
        }

        /* Decode path into a stack buffer BEFORE body reading, because
         * read_chunked_body may compact rbuf (overwriting the request-line
         * area that req.target points into). */
        bool is_get  = scl_strcmp(req.method, "GET") == 0;
        bool is_head = scl_strcmp(req.method, "HEAD") == 0;
        head_only = is_head;
        char pathbuf[SCL_PATH_MAX];
        {
            const char *q = scl_strchr(req.target, '?');
            size_t rawlen = q ? (size_t)(q - req.target) : scl_strlen(req.target);
            long dl = url_decode_path(req.target, rawlen, pathbuf, sizeof(pathbuf));
            if (scl_unlikely(dl < 0)) {
                send_error(srv, fd, 400, head_only, true);
                return;
            }
            req.path = pathbuf;
        }

        /* Read body */
        void *body_buf = NULL;
        size_t body_len = 0;

        if (is_chunked) {
            status = read_chunked_body(alloc, fd, conn->rbuf, &conn->rbuf_len,
                                       conn->rbuf_cap, hend,
                                       max_body, &body_buf, &body_len);
            if (status != 0) {
                send_error(srv, fd, status, false, true);
                return;
            }
            body_allocated = true;
        } else if (valid_cl && content_length > 0) {
            status = read_body_fixed(alloc, fd, conn->rbuf, &conn->rbuf_len, hend,
                                     content_length, max_body, &body_buf, &body_len);
            if (status != 0) {
                send_error(srv, fd, status, false, true);
                return;
            }
            body_allocated = true;
        }

        req.body = body_buf;
        req.body_len = body_len;

        bool is_get  = scl_strcmp(req.method, "GET") == 0;
        bool is_head = scl_strcmp(req.method, "HEAD") == 0;
        head_only = is_head;

        will_close = !req.keep_alive || (served + 1 >= srv->cfg.keep_alive_max);

        /* Parse multipart/form-data if applicable */
        if (body_len > 0 && req.content_type &&
            scl_strstr(req.content_type, "multipart/form-data")) {
            size_t upcount = 0;
            scl_http_parse_multipart(req.body, req.body_len, req.content_type,
                                     req.uploads, &upcount, SCL_HTTP_MAX_UPLOADS);
            req.upload_count = upcount;
        }

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

        /* Free body buffer if allocated */
        if (body_allocated) scl_free(alloc, body_buf);

        if (serr != SCL_OK || will_close) return;

        /* Compact consumed bytes from rbuf.
         *
         * read_body_fixed already compacts body bytes (head-offset = hend+4+cl).
         * read_chunked_body also compacts — both preserve any pipelined bytes.
         * Only no-body requests still have headers (hend+4) in rbuf that must
         * be removed now so the next iteration sees a fresh request at index 0.
         */
        if (!body_allocated) {
            size_t hdr_sz = hend + 4;
            if (conn->rbuf_len > hdr_sz) {
                scl_memmove(conn->rbuf, conn->rbuf + hdr_sz,
                            conn->rbuf_len - hdr_sz);
                conn->rbuf_len -= hdr_sz;
            } else {
                conn->rbuf_len = 0;
            }
        }
        /* else: read_body_fixed already compacted — do nothing */
        served++;
    }
}

/*
 * ── Worker / acceptor threads ─────────────────────────────────────
 */
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
    while (atomic_load_explicit(&srv->running, memory_order_acquire)) {
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

    while (atomic_load_explicit(&srv->running, memory_order_acquire)) {
        int pr = poll(&pfd, 1, 200);
        if (pr <= 0) continue;

        for (;;) {
            scl_tcp_conn_t tmp;
            tmp.fd = -1;
            scl_error_t aerr = scl_tcp_accept(srv->listen_fd, &tmp);
            if (aerr == SCL_ERR_TIMEOUT) break;
            if (aerr != SCL_OK) break;

            scl_tcp_conn_t *conn = scl_tcp_pool_acquire(&srv->pool);
            if (!conn) { close(tmp.fd); continue; }

            conn->fd       = tmp.fd;
            conn->peer     = tmp.peer;
            conn->peer_len = tmp.peer_len;
            conn->rbuf_len = 0;
            scl_tcp_set_recv_timeout(conn->fd, srv->cfg.recv_timeout_ms);

            if (!scl_tcp_pool_post_ready(&srv->pool, conn)) {
                scl_tcp_pool_release(&srv->pool, conn);
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
    if (c->max_body_size == 0) c->max_body_size = SCL_HTTP_MAX_BODY_DEFAULT;
    if (c->recv_timeout_ms == 0) c->recv_timeout_ms = 15000;
    if (c->keep_alive_max <= 0) c->keep_alive_max = 100;
    if (c->backlog       <= 0) c->backlog       = 128;
    if (!c->server_name)       c->server_name   = "scl-httpd";
}

scl_error_t scl_http_server_init(scl_allocator_t *alloc, scl_http_server_t **out,
                                 const scl_http_config_t *cfg) {
    if (scl_unlikely(!alloc || !out || !cfg)) return SCL_ERR_NULL_PTR;
    *out = NULL;

    scl_net_init();

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

    atomic_store_explicit(&srv->running, true, memory_order_release);

    for (int i = 0; i < srv->num_workers; i++) {
        if (scl_thread_create(&srv->workers[i], worker_main, srv) != SCL_OK) {
            atomic_store_explicit(&srv->running, false, memory_order_release);
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
    if (!atomic_exchange_explicit(&srv->running, false, memory_order_acq_rel)) {
        if (!atomic_load(&srv->started)) return SCL_OK;
    }

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
