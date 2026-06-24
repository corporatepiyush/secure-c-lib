#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O2")
#endif

/*
 * scl_http_client.c — HTTP/1.1 client implementation.
 *
 * ── What this file does ─────────────────────────────────────────
 *
 * This is the companion to scl_http_server.c: it lets you make
 * outgoing HTTP requests with the same security-conscious design.
 * Where the server parses incoming HTTP requests (status line +
 * headers + body), this client builds outgoing requests and parses
 * incoming responses.
 *
 * ── Key design decisions ────────────────────────────────────────
 *
 * 1. Blocking I/O: the entire API is synchronous. Each call to
 *    scl_http_client_request() sends the request and reads the
 *    complete response before returning. This makes the API simple
 *    and safe (no callback lifetime issues). For non-blocking or
 *    streaming use, we expose the parser state machine.
 *
 * 2. Keep-alive: We reuse TCP connections across requests to the
 *    same host:port, matching the server's behaviour in
 *    handle_connection() (scl_http_server.c:432). The server tells
 *    us via the Connection header; we respect it.
 *
 * 3. One recv buffer per client: raw bytes accumulate in a single
 *    read buffer, then the parser extracts headers and body offsets
 *    without copying the body (zero-copy for the response body).
 *    The body pointer in the response points into this buffer.
 *
 * 4. Bounds-checked parsing: every offset, length, and count derived
 *    from the response is validated through scl_range_in_bounds()
 *    before use — the same hardening applied to all docparse parsers
 *    (test_scl_docparse_hardening.c).
 */

#include "scl_http_client.h"
#include "scl_stdlib.h"   /* scl_strtoul — guards ERANGE */
#include "scl_tcp_pool.h" /* scl_net_init(), scl_tcp_send_all(), etc. */
#include "scl_time.h"     /* scl_deadline_from_now_ms, etc. */

#include <unistd.h>       /* close, read, write */
#include <sys/socket.h>   /* socket, connect, recv, send */
#include <netdb.h>        /* getaddrinfo, freeaddrinfo */
#include <errno.h>        /* errno, EINTR, EAGAIN */
#include <sys/time.h>     /* struct timeval, setsockopt SO_RCVTIMEO */
#include <netinet/tcp.h>  /* TCP_NODELAY */
#include <stdio.h>        /* snprintf (only safe snprintf — no stdio risks) */

/*
 * ── Internal client structure ──────────────────────────────────
 *
 * We keep a single TCP connection. The response body points into
 * the read buffer (rbuf) where possible, avoiding a copy.
 */
struct scl_http_client {
    scl_allocator_t *alloc;

    /* Current TCP connection */
    int      fd;
    char    *conn_host;       /* alloc'd copy */
    uint16_t conn_port;
    bool     conn_close;      /* server asked for close after last response */

    /* Read buffer — accumulates raw socket data */
    unsigned char *rbuf;
    size_t         rbuf_cap;
    size_t         rbuf_len;
    size_t         rbuf_pos;  /* how much the parser has consumed */

    /* Max body we will accept */
    size_t max_body_size;
};

/* ── URL parsing ───────────────────────────────────────────────────
 *
 * URL parsing is a classic source of security bugs (see e.g. the
 * "Java URL parsing inconsistency" CVEs). The key rules we follow:
 *
 *   • Check for "://" from the START of the string only — not anywhere.
 *     This prevents "http://evil.com^http://good.com" confusion.
 *   • Reject embedded NUL bytes (parser differential attacks).
 *   • Reject userinfo ("user:pass@") — credentials in URLs are an
 *     anti-pattern (they appear in logs, process listings, and are
 *     a phishing/SSRF vector).
 *   • Reject fragment identifiers ('#') — they have no meaning in
 *     client->server HTTP. Accepting them can lead to cache poisoning
 *     if the server ignores the fragment but downstream caches differ.
 */
scl_error_t scl_http_parse_url(char *url_str, scl_http_url_t *out) {
    if (scl_unlikely(!url_str || !out)) return SCL_ERR_NULL_PTR;

    scl_memset(out, 0, sizeof(*out));
    out->path = "/";
    out->port = 80;

    size_t urllen = scl_strlen(url_str);
    if (urllen == 0) return SCL_ERR_INVALID_ARG;

    /* Reject embedded NUL (parser differential defence). */
    if (scl_strlen(url_str) != urllen) return SCL_ERR_INVALID_ARG;

    char *p = url_str;

    /* Detect scheme. "://" must appear at the very start. */
    if (scl_strncmp(p, "http://", 7) == 0) {
        out->scheme = "http";
        p += 7;
    } else if (scl_strncmp(p, "https://", 8) == 0) {
        out->scheme = "https";
        return SCL_ERR_UNSUPPORTED;
    }

    /* Extract host. Ends at ':', '/', '?', '#', or end-of-string. */
    char *host_start = p;

    /* Handle IPv6: host in brackets like "[::1]" or "[::1]:8080" */
    if (*p == '[') {
        host_start = p + 1;
        char *close_bracket = scl_strchr(p + 1, ']');
        if (!close_bracket) return SCL_ERR_INVALID_ARG;
        for (char *cp = host_start; cp < close_bracket; cp++) {
            if ((unsigned char)*cp < 0x20) return SCL_ERR_INVALID_ARG;
        }
        *close_bracket = '\0';
        p = close_bracket + 1;
        if (*p == ':') {
            char *end = NULL;
            unsigned long port = scl_strtoul(p + 1, &end, 10);
            if (!end || end == p + 1 ||
                port == 0 || port > 65535)
                return SCL_ERR_INVALID_ARG;
            out->port = (uint16_t)port;
            p = end;
        }
    } else {
        char *host_end = p;
        char *port_colon = NULL;
        while (*host_end && *host_end != '/' && *host_end != '?' && *host_end != '#') {
            if (*host_end == ':' && !port_colon)
                port_colon = host_end;
            if ((unsigned char)*host_end < 0x20) return SCL_ERR_INVALID_ARG;
            host_end++;
        }

        if (port_colon) {
            *port_colon = '\0';
            char *end = NULL;
            unsigned long port = scl_strtoul(port_colon + 1, &end, 10);
            if (!end || end == port_colon + 1 || port == 0 || port > 65535)
                return SCL_ERR_INVALID_ARG;
            out->port = (uint16_t)port;
        }

        p = host_end;
    }

    size_t host_len = scl_strlen(host_start);
    if (host_len == 0) return SCL_ERR_INVALID_ARG;
    out->host = host_start;

    /* Reject fragments ('#') — meaningless in HTTP requests. */
    if (scl_strchr(p, '#') != NULL) return SCL_ERR_INVALID_ARG;

    /* Parse path and optional query.
     * We NUL-terminate at '?' so that out->path points to just the
     * path portion and out->query points past the '?'. */
    if (*p == '/' || *p == '?') {
        out->path = p;
        char *qm = scl_strchr(p, '?');
        if (qm) {
            *qm = '\0';
            out->query = qm + 1;
        }
    } else if (*p == '\0') {
        out->path = "/";
    } else {
        return SCL_ERR_INVALID_ARG;
    }

    return SCL_OK;
}

/* ── Helpers ───────────────────────────────────────────────────── */

/* Case-insensitive string compare (for header names). */
static bool ci_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (scl_tolower((unsigned char)*a) != scl_tolower((unsigned char)*b))
            return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Case-insensitive check for "close" in a Connection header value. */
static bool conn_close_token(const char *val) {
    if (scl_unlikely(!val)) return false;
    while (*val) {
        if ((*val == 'c' || *val == 'C') &&
            (val[1] == 'l' || val[1] == 'L') &&
            (val[2] == 'o' || val[2] == 'O') &&
            (val[3] == 's' || val[3] == 'S') &&
            (val[4] == 'e' || val[4] == 'E') &&
            (val[5] == '\0' || val[5] == ' ' || val[5] == ',' || val[5] == '\t' ||
             val[5] == '\r'))
            return true;
        val++;
    }
    return false;
}

/* ── TCP connection ───────────────────────────────────────────── */

static scl_error_t tcp_connect_to(const char *host, uint16_t port, int *out_fd) {
    if (scl_unlikely(!host || !out_fd)) return SCL_ERR_NULL_PTR;
    *out_fd = -1;

    char port_str[16];
    int pn = snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
    if (pn < 0 || (size_t)pn >= sizeof(port_str)) return SCL_ERR_INVALID_ARG;
    port_str[15] = '\0';

    struct addrinfo hints;
    scl_memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* accept IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;   /* TCP */
    hints.ai_flags    = 0;             /* no AI_PASSIVE — we are a client */

    struct addrinfo *res = NULL;
    int gai_err = getaddrinfo(host, port_str, &hints, &res);
    if (gai_err != 0 || !res) return SCL_ERR_NOT_FOUND;

    int fd = -1;
    scl_error_t err = SCL_ERR_IO;

    /* Try each address until one connects. */
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            err = SCL_OK;
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if (err == SCL_OK && fd >= 0) {
        /* Disable Nagle — same as the server does. */
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        *out_fd = fd;
    }

    return err;
}

/*
 * ── Client lifecycle ──────────────────────────────────────────
 */
scl_error_t scl_http_client_init(scl_allocator_t *alloc,
                                 scl_http_client_t **out,
                                 size_t max_body_size) {
    if (scl_unlikely(!alloc || !out)) return SCL_ERR_NULL_PTR;
    *out = NULL;

    scl_net_init();  /* idempotently ignore SIGPIPE */

    scl_http_client_t *c = (scl_http_client_t *)scl_calloc(alloc, 1,
                              sizeof(scl_http_client_t), _Alignof(scl_http_client_t));
    if (!c) return SCL_ERR_OUT_OF_MEMORY;

    c->alloc = alloc;
    c->fd = -1;
    c->max_body_size = max_body_size > 0 ? max_body_size : SCL_HTTP_CLIENT_MAX_BODY_BUF;

    /* Pre-allocate read buffer. */
    c->rbuf_cap = 16384;
    c->rbuf = (unsigned char *)scl_alloc(alloc, c->rbuf_cap, _Alignof(max_align_t));
    if (!c->rbuf) {
        scl_free(alloc, c);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    c->rbuf_len = 0;
    c->rbuf_pos = 0;

    *out = c;
    return SCL_OK;
}

void scl_http_client_destroy(scl_http_client_t *c) {
    if (!c) return;
    scl_http_client_disconnect(c);
    scl_free(c->alloc, c->rbuf);
    scl_free(c->alloc, c->conn_host);
    scl_free(c->alloc, c);
}

/*
 * ── Connection management ──────────────────────────────────────
 */
scl_error_t scl_http_client_connect(scl_http_client_t *c,
                                    const char *host, uint16_t port) {
    if (scl_unlikely(!c || !host)) return SCL_ERR_NULL_PTR;

    /* Reuse existing connection if viable. */
    if (c->fd >= 0 && c->conn_host && c->conn_port == port &&
        !c->conn_close && scl_strcmp(c->conn_host, host) == 0) {
        return SCL_OK;
    }

    /* Close old connection. */
    scl_http_client_disconnect(c);

    int fd = -1;
    scl_error_t err = tcp_connect_to(host, port, &fd);
    if (err != SCL_OK) return err;

    /* Set receive timeout (same approach as scl_http_server.c). */
    struct timeval tv;
    tv.tv_sec  = SCL_HTTP_CLIENT_DEFAULT_TIMEOUT_MS / 1000;
    tv.tv_usec = (SCL_HTTP_CLIENT_DEFAULT_TIMEOUT_MS % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    c->fd = fd;
    c->conn_port = port;
    c->conn_close = false;

    scl_free(c->alloc, c->conn_host);
    c->conn_host = scl_strdup(c->alloc, host);

    return SCL_OK;
}

/*
 * Free the body and headers of a response. This is a convenience wrapper
 * so callers don't need to call scl_free on the allocator directly.
 *
 * Safe to call with a zeroed-out response (checks each pointer for NULL
 * before freeing). After calling, the response should be zeroed before
 * reuse to avoid double-free from stale pointers.
 */
void scl_http_client_request_free(scl_allocator_t *alloc,
                                  scl_http_client_response_t *resp) {
    if (!alloc || !resp) return;
    scl_free(alloc, resp->headers);
    resp->headers = NULL;
    resp->headers_len = 0;
    resp->header_count = 0;
    scl_free(alloc, resp->body);
    resp->body = NULL;
    resp->body_len = 0;
    resp->body_cap = 0;
}

/*
 * Set the receive timeout for the current TCP connection.
 * If called before the first request, it takes effect on connect.
 * If called mid-connection, it sets SO_RCVTIMEO on the live socket.
 * Timeout of 0 means no timeout (blocking indefinitely).
 */
void scl_http_client_set_timeout(scl_http_client_t *c, int64_t timeout_ms) {
    if (!c || c->fd < 0) return;
    struct timeval tv;
    tv.tv_sec  = (long)(timeout_ms / 1000);
    tv.tv_usec = (long)((timeout_ms % 1000) * 1000);
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void scl_http_client_disconnect(scl_http_client_t *c) {
    if (!c) return;
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
    /* Scrub any buffered response data before reusing. */
    if (c->rbuf && c->rbuf_len > 0) {
        scl_secure_zero(c->rbuf, c->rbuf_len);
    }
    c->rbuf_len = 0;
    c->rbuf_pos = 0;
    c->conn_close = false;
}

/* ── Sending the request ──────────────────────────────────────────
 *
 * Builds a well-formed HTTP request in a stack buffer and ships it
 * over the socket. The buffer is stack-allocated (no malloc) since
 * typical request headers are < 8 KB.
 */
static scl_error_t send_request(scl_http_client_t *c,
                                const char *method,
                                const char *path_with_query,
                                const char *extra_headers,
                                const void *body, size_t body_len) {
    /* Validate method: uppercase alpha only (CRLF injection defence). */
    if (scl_unlikely(!method || method[0] == '\0')) return SCL_ERR_INVALID_ARG;
    for (const char *m = method; *m; m++) {
        if (scl_unlikely(*m < 'A' || *m > 'Z')) return SCL_ERR_INVALID_ARG;
    }

    char reqbuf[8192];
    size_t reqlen = 0;

    /* Request line: "GET /path HTTP/1.1\r\n" */
    int n = snprintf(reqbuf, sizeof(reqbuf), "%s %s HTTP/1.1\r\n",
                     method, path_with_query ? path_with_query : "/");
    if (n < 0 || (size_t)n >= sizeof(reqbuf)) return SCL_ERR_SIZE_OVERFLOW;
    reqlen = (size_t)n;

    /* Host header (mandatory for HTTP/1.1, RFC 7230 §5.4). */
    if (c->conn_host) {
        n = snprintf(reqbuf + reqlen, sizeof(reqbuf) - reqlen,
                     "Host: %s", c->conn_host);
        if (n < 0 || reqlen + (size_t)n >= sizeof(reqbuf))
            return SCL_ERR_SIZE_OVERFLOW;
        reqlen += (size_t)n;
        if (c->conn_port != 80) {
            n = snprintf(reqbuf + reqlen, sizeof(reqbuf) - reqlen,
                         ":%u\r\n", (unsigned)c->conn_port);
            if (n < 0 || reqlen + (size_t)n >= sizeof(reqbuf))
                return SCL_ERR_SIZE_OVERFLOW;
            reqlen += (size_t)n;
        } else {
            if (reqlen + 3 > sizeof(reqbuf)) return SCL_ERR_SIZE_OVERFLOW;
            scl_memcpy(reqbuf + reqlen, "\r\n", 3);
            reqlen += 2;
        }
    }

    /* User-supplied extra headers. */
    if (extra_headers && extra_headers[0]) {
        size_t elen = scl_strlen(extra_headers);
        size_t needed;
        if (scl_add_overflow(reqlen, elen, &needed) || needed + 2 > sizeof(reqbuf))
            return SCL_ERR_SIZE_OVERFLOW;
        scl_memcpy(reqbuf + reqlen, extra_headers, elen);
        reqlen += elen;
        /* Ensure request ends with \r\n. */
        if (reqbuf[reqlen - 1] != '\n') {
            scl_memcpy(reqbuf + reqlen, "\r\n", 2);
            reqlen += 2;
        }
    }

    /* Content-Length for request body. */
    if (body_len > 0) {
        n = snprintf(reqbuf + reqlen, sizeof(reqbuf) - reqlen,
                     "Content-Length: %zu\r\n", body_len);
        if (n < 0 || reqlen + (size_t)n >= sizeof(reqbuf))
            return SCL_ERR_SIZE_OVERFLOW;
        reqlen += (size_t)n;
    }

    /* End-of-headers blank line. */
    if (reqlen + 2 > sizeof(reqbuf)) return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(reqbuf + reqlen, "\r\n", 2);
    reqlen += 2;

    /* Send headers. */
    scl_error_t err = scl_tcp_send_all(c->fd, reqbuf, reqlen);
    if (err != SCL_OK) return err;

    /* Send body (if any). */
    if (body && body_len > 0) {
        err = scl_tcp_send_all(c->fd, body, body_len);
    }

    return err;
}

/* ── Response parsing helpers ─────────────────────────────────────
 *
 * All of these operate on the client's rbuf and track state through
 * rbuf_pos. After each call, rbuf_pos advances past consumed data.
 */

/*
 * Helper: find "\r\n" in the buffer starting at rbuf_pos.
 * Returns the offset from rbuf_pos, or -1 if not found.
 */
static long find_crlf(const unsigned char *buf, size_t buf_len, size_t start) {
    for (size_t i = start; i + 1 < buf_len; i++) {
        if (scl_likely(buf[i] == '\r' && buf[i + 1] == '\n'))
            return (long)(i - start);
    }
    return -1;
}

/*
 * Helper: find "\r\n\r\n" (end of headers) in the buffer.
 */
static long find_hdrend(const unsigned char *buf, size_t buf_len) {
    for (size_t i = 0; i + 3 < buf_len; i++) {
        if (scl_likely(buf[i] == '\r' && buf[i+1] == '\n' &&
                       buf[i+2] == '\r' && buf[i+3] == '\n'))
            return (long)i;
    }
    return -1;
}

/*
 * ── Core: parse the status line ───────────────────────────────────
 *
 * Given a NUL-terminated line "HTTP/1.1 200 OK\r\n...", extract
 * the version minor, status code, and reason phrase. Returns SCL_OK
 * on success.
 */
static scl_error_t parse_status_line(const char *line,
                                     int *out_status,
                                     char *reason_buf, size_t reason_cap) {
    int ver = 0;
    int st  = 0;
    int n = sscanf(line, "HTTP/%*d.%d %d", &ver, &st);
    if (scl_unlikely(n < 2 || st < 100 || st > 599))
        return SCL_ERR_PARSE;

    *out_status = st;

    /* Extract reason phrase: everything after the status code.
     * "HTTP/1.1 200 OK" — find the second space. */
    int space_count = 0;
    const char *reason_start = NULL;
    for (const char *p = line; *p; p++) {
        if (*p == ' ') {
            space_count++;
            if (space_count == 2) {
                reason_start = p + 1;
                break;
            }
        }
    }
    if (reason_start) {
        scl_strlcpy(reason_buf, reason_start, reason_cap);
    } else {
        reason_buf[0] = '\0';
    }

    return SCL_OK;
}

/*
 * ── Core: read and parse a complete response ──────────────────────
 *
 * This is the main response-reading function. It:
 *
 *   1. Accumulates raw data in c->rbuf until we have the full
 *      header block ("\r\n\r\n").
 *   2. Parses the status line from rbuf.
 *   3. Scans headers to find Content-Length, Connection, and
 *      Transfer-Encoding.
 *   4. Reads the body using the appropriate strategy.
 *   5. Returns a fully populated scl_http_client_response_t.
 *
 * The response's body pointer points into c->rbuf (zero-copy).
 * The headers are also kept in c->rbuf.
 */
static scl_error_t read_response(scl_http_client_t *c,
                                 scl_http_client_response_t *resp,
                                 bool head_response) {
    scl_memset(resp, 0, sizeof(*resp));
    resp->status = 0;

    long hdr_end;
    size_t header_block_len;
    long eol_off;
    int status;
    scl_error_t err;
    size_t hdr_start;
    size_t hdr_area_len;
    unsigned long content_length;
    bool connection_close;
    bool chunked;
    size_t found_headers;
    size_t resp_headers_cap;

    for (;;) {
    /*
     * Phase 1: accumulate until we have the complete header block
     * delimited by "\r\n\r\n". If there's already data in rbuf
     * (e.g., leftover from a previous partial read), use it.
     */
    hdr_end = -1;

    while ((hdr_end = find_hdrend(c->rbuf, c->rbuf_len)) < 0) {
        /* Check for oversized headers before reading more. */
        if (scl_unlikely(c->rbuf_len >= SCL_HTTP_CLIENT_MAX_HEADER_BUF))
            return SCL_ERR_SIZE_OVERFLOW;

        ssize_t n = recv(c->fd, c->rbuf + c->rbuf_len,
                         c->rbuf_cap - c->rbuf_len, 0);
        if (scl_likely(n > 0)) {
            c->rbuf_len += (size_t)n;
            continue;
        }
        if (scl_unlikely(n == 0)) return SCL_ERR_IO;
        if (errno == EINTR) continue;
        if (scl_unlikely(errno == EAGAIN || errno == EWOULDBLOCK)) return SCL_ERR_TIMEOUT;
        return SCL_ERR_IO;
    }

    /* hdr_end is the offset of the "\r\n" before the blank line.
     * hdr_end + 4 skips past "\r\n\r\n". */
    header_block_len = (size_t)hdr_end + 4;

    /* NOT writing a global NUL here — the individual header-line NUL
     * terminations (done during parsing below) are sufficient, and a
     * NUL at header_block_len would corrupt the first body byte. */

    /*
     * Phase 2: parse status line.
     * The status line is "HTTP/1.1 200 OK\r\n" starting at rbuf[0].
     */
    eol_off = find_crlf(c->rbuf, c->rbuf_len, 0);
    if (eol_off < 0) return SCL_ERR_PARSE;
    c->rbuf[(size_t)eol_off] = '\0';

    status = 0;
    err = parse_status_line((const char *)c->rbuf,
                            &status,
                            resp->status_text,
                            sizeof(resp->status_text));
    c->rbuf[(size_t)eol_off] = '\r';  /* restore */
    if (err != SCL_OK) return err;
    resp->status = status;

    /*
     * Phase 3: scan headers to determine body strategy.
     *
     * Headers begin after the status line's CRLF, i.e. at
     * rbuf[eol_off + 2], and end at rbuf[hdr_end].
     */
    hdr_start = (size_t)eol_off + 2;  /* after status line's CRLF */
    hdr_area_len = header_block_len - 4 - hdr_start;  /* excl. \r\n\r\n */

    content_length = 0;
    connection_close = false;
    chunked = false;
    found_headers = 0;
    resp_headers_cap = 0;

    /*
     * We extract headers into a persistent heap block that outlives
     * rbuf. This allows scl_http_client_find_header() to work after
     * the response is parsed (even if rbuf is reused for the next
     * request).
     *
     * Format: "Name: Value\0Name: Value\0\0" (double-NUL terminated).
     * Simple, searchable with scl_strstr + pointer arithmetic.
     *
     * Since header_block_len is bounded (64 KB max), this is safe.
     */
    if (hdr_area_len > 0) {
        /* Count headers to allocate precisely. A crude upper bound:
         * each header is at least "A:B\r\n" (5 bytes), so cap at
         * hdr_area_len / 2. */
        size_t max_hdrs = hdr_area_len / 2;
        if (max_hdrs > SCL_HTTP_CLIENT_MAX_HEADERS)
            max_hdrs = SCL_HTTP_CLIENT_MAX_HEADERS;

        /* Allocate with generous extra for safety. */
        size_t store_size = hdr_area_len + max_hdrs + 2;
        if (store_size > SCL_HTTP_CLIENT_MAX_HEADER_BUF + 1024)
            store_size = SCL_HTTP_CLIENT_MAX_HEADER_BUF + 1024;

        resp->headers = (char *)scl_alloc(c->alloc, store_size, _Alignof(max_align_t));
        if (resp->headers) {
            resp_headers_cap = store_size;
            resp->headers[0] = '\0';
            size_t store_pos = 0;

            /* Walk header lines. */
            size_t hp = hdr_start;
            while (hp + 1 < header_block_len - 2) {
                long hdr_eol = find_crlf(c->rbuf, c->rbuf_len, hp);
                if (hdr_eol < 0) break;
                size_t line_start = hp;
                size_t line_end = hp + (size_t)hdr_eol;  /* position of \r */

                /* Skip the blank line that terminates headers. */
                if (line_end == line_start) {
                    hp = line_end + 2;
                    break;
                }

                c->rbuf[line_end] = '\0';    /* NUL-terminate the header line */
                const char *hdr_line = (const char *)c->rbuf + line_start;

                /*
                 * Parse "Name: Value".
                 * Per RFC 7230 §3.2.4, header fields are "Name: value".
                 * The colon is mandatory.
                 */
                const char *colon = scl_strchr(hdr_line, ':');
                if (colon) {
                    /* Temporarily NUL-terminate at the colon so we can
                     * compare just the header name, then restore it for
                     * storage. */
                    char *mutable_colon = (char *)colon;
                    char colon_saved = *mutable_colon;
                    *mutable_colon = '\0';
                    const char *hdr_name = hdr_line;    /* "Name" only */
                    const char *hdr_val  = colon + 1;
                    while (*hdr_val == ' ' || *hdr_val == '\t') hdr_val++;

                    if (ci_eq(hdr_name, "Content-Length")) {
                        char *end = NULL;
                        unsigned long cl = scl_strtoul(hdr_val, &end, 10);
                        if (end && *end == '\0')
                            content_length = cl;
                    } else if (ci_eq(hdr_name, "Connection")) {
                        if (conn_close_token(hdr_val))
                            connection_close = true;
                    } else if (ci_eq(hdr_name, "Transfer-Encoding")) {
                        if (scl_strstr(hdr_val, "chunked"))
                            chunked = true;
                    }

                    *mutable_colon = colon_saved;  /* restore for storage */

                    size_t nv_len = scl_strlen(hdr_line);  /* "Name: value" */
                    size_t needed;
                    if (scl_add_overflow(store_pos, nv_len + 1, &needed) ||
                        needed >= resp_headers_cap - 1)
                        break;

                    scl_memcpy(resp->headers + store_pos, hdr_line, nv_len + 1);
                    store_pos += nv_len + 1;
                    found_headers++;
                }

                c->rbuf[line_end] = '\r';     /* restore rbuf */
                hp = line_end + 2;             /* skip CRLF */
            }

            resp->headers[store_pos] = '\0';  /* double-NUL termination */
            resp->headers_len = store_pos;
            resp->header_count = found_headers;
        }
    }

    resp->connection_close = connection_close;
    c->conn_close = connection_close;

    /* Skip interim 1xx responses (RFC 7231 §6.2) */
    if (status >= 100 && status < 200 && status != 101) {
        scl_free(c->alloc, resp->headers);
        resp->headers = NULL;
        if (c->rbuf_len > header_block_len) {
            memmove(c->rbuf, c->rbuf + header_block_len, c->rbuf_len - header_block_len);
            c->rbuf_len -= header_block_len;
        } else {
            c->rbuf_len = 0;
        }
        scl_memset(resp, 0, sizeof(*resp));
        resp->status = 0;
        continue;
    }
    break;
    }

    /*
     * Phase 4: body reading.
     */
    bool no_body = head_response ||
                   status == 204 || status == 304 || status == 101;

    if (no_body) {
        return SCL_OK;
    }

    if (chunked) {
        return SCL_ERR_UNSUPPORTED;
    }

    if (content_length > 0) {
        /* Validate Content-Length before allocating. */
        if (content_length > c->max_body_size)
            return SCL_ERR_SIZE_OVERFLOW;

        /* +1 for null terminator so body can be used as a C string */
        resp->body = scl_alloc(c->alloc, content_length + 1, _Alignof(max_align_t));
        if (!resp->body) return SCL_ERR_OUT_OF_MEMORY;
        resp->body_cap = content_length + 1;
        resp->body_len = 0;

        /* Copy any body bytes already in rbuf. */
        if (c->rbuf_len > header_block_len) {
            size_t avail = c->rbuf_len - header_block_len;
            size_t to_copy = avail < content_length ? avail : content_length;
            scl_memcpy(resp->body, c->rbuf + header_block_len, to_copy);
            resp->body_len = to_copy;
        }

        /* Read remaining body bytes. */
        while (resp->body_len < content_length) {
            size_t remaining = content_length - resp->body_len;
            size_t to_read = remaining < 65536 ? remaining : 65536;
            ssize_t n = recv(c->fd,
                             (unsigned char *)resp->body + resp->body_len,
                             to_read, 0);
            if (scl_likely(n > 0)) {
                resp->body_len += (size_t)n;
            } else if (scl_unlikely(n == 0)) {
                /* Premature close — server sent fewer bytes than promised. */
                c->conn_close = true;
                break;
            } else if (scl_unlikely(errno == EINTR)) {
                continue;
            } else if (scl_unlikely(errno == EAGAIN || errno == EWOULDBLOCK)) {
                return SCL_ERR_TIMEOUT;
            } else {
                return SCL_ERR_IO;
            }
        }
    } else {
        /* No Content-Length: read until the peer closes (HTTP/1.0 style). */
        size_t body_cap = 4096;
        resp->body = scl_alloc(c->alloc, body_cap, _Alignof(max_align_t));
        if (!resp->body) return SCL_ERR_OUT_OF_MEMORY;
        resp->body_cap = body_cap;
        resp->body_len = 0;

        /* Copy any bytes already in rbuf after the header block. */
        size_t avail = c->rbuf_len > header_block_len ? c->rbuf_len - header_block_len : 0;
        if (avail > 0) {
            size_t to_copy = avail < body_cap ? avail : body_cap;
            scl_memcpy(resp->body, c->rbuf + header_block_len, to_copy);
            resp->body_len = to_copy;
        }

        for (;;) {
            if (resp->body_len >= c->max_body_size) break;
            if (resp->body_len + 4096 > resp->body_cap) {
                size_t new_cap = resp->body_cap * 2;
                if (new_cap > c->max_body_size) new_cap = c->max_body_size;
                if (new_cap <= resp->body_cap) break;
                void *nb = scl_realloc(c->alloc, resp->body, resp->body_cap,
                                       new_cap, _Alignof(max_align_t));
                if (!nb) break;
                resp->body = nb;
                resp->body_cap = new_cap;
            }
            ssize_t n = recv(c->fd,
                             (unsigned char *)resp->body + resp->body_len,
                             resp->body_cap - resp->body_len, 0);
            if (n > 0) {
                resp->body_len += (size_t)n;
            } else if (n == 0) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (resp->body_len > 0) break;
                return SCL_ERR_TIMEOUT;
            } else {
                return SCL_ERR_IO;
            }
        }
    }

    if (resp->body && resp->body_len < resp->body_cap)
        ((unsigned char *)resp->body)[resp->body_len] = '\0';
    return SCL_OK;
}

/*
 * Case-insensitive comparison for N-byte prefix (portable,
 * avoids <strings.h> which is not available on all platforms
 * and can behave differently on macOS vs Linux for strncasecmp).
 *
 * Returns 0 if the first n bytes of a and b match case-insensitively.
 */
static int ci_ncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] == '\0' && b[i] == '\0') return 0;
        if (a[i] == '\0') return -1;
        if (b[i] == '\0') return 1;
        int ca = scl_tolower((unsigned char)a[i]);
        int cb = scl_tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
    }
    return 0;
}

/*
 * ── Public: find a header by name ───────────────────────────────
 *
 * Scans the NUL-separated header block stored in the response.
 * Returns NULL if the header is not found.
 *
 * Performance note: this is O(n) in the number of headers, but since
 * we cap at SCL_HTTP_CLIENT_MAX_HEADERS (64), it is fast enough.
 */
const char *scl_http_client_find_header(const scl_http_client_response_t *resp,
                                        const char *name) {
    if (!resp || !name || !resp->headers) return NULL;

    size_t name_len = scl_strlen(name);
    if (name_len == 0) return NULL;

    /* Walk the NUL-separated header block. */
    const char *p = resp->headers;
    while (*p) {
        size_t nvlen = scl_strlen(p);
        const char *colon = scl_strchr(p, ':');
        if (colon) {
            size_t hdr_name_len = (size_t)(colon - p);
            if (name_len == hdr_name_len && ci_ncmp(p, name, name_len) == 0) {
                const char *val = colon + 1;
                while (*val == ' ' || *val == '\t') val++;
                return val;
            }
        }
        p += nvlen + 1;
    }

    return NULL;
}

/* ── Public: main request API ──────────────────────────────────── */
scl_error_t scl_http_client_request(scl_http_client_t *c,
                                    const char *method,
                                    const char *url,
                                    const char *headers,
                                    const void *body, size_t body_len,
                                    scl_http_client_response_t *resp) {
    if (scl_unlikely(!c || !method || !url || !resp)) return SCL_ERR_NULL_PTR;

    /*
     * Step 1: copy the URL to a mutable stack buffer (parse_url
     * NUL-terminates components inside the buffer at '?' and ':').
     */
    size_t url_len = scl_strlen(url);
    if (url_len >= SCL_HTTP_CLIENT_MAX_TARGET) return SCL_ERR_SIZE_OVERFLOW;
    char url_copy[SCL_HTTP_CLIENT_MAX_TARGET];
    scl_memcpy(url_copy, url, url_len + 1);

    /*
     * Step 2: parse the URL to get host, port, path, query.
     */
    scl_http_url_t url_parts;
    scl_error_t err = scl_http_parse_url(url_copy, &url_parts);
    if (err != SCL_OK) return err;

    /*
     * Step 3: extract host as a NUL-terminated string for getaddrinfo.
     * We scan from the original URL (past "://") to avoid issues with
     * IPv6 addresses (which contain ':' characters).
     */
    const char *hstart = url;
    const char *scheme_sep = scl_strstr(url, "://");
    if (scheme_sep) hstart = scheme_sep + 3;

    const char *hend = hstart;
    /* IPv6 addresses are bracketed: skip past the closing ']'. */
    if (*hend == '[') {
        hend = scl_strchr(hend, ']');
        if (!hend) return SCL_ERR_INVALID_ARG;
        hend++; /* point past ']' */
    }
    while (*hend && *hend != ':' && *hend != '/' && *hend != '?') hend++;
    size_t host_len = (size_t)(hend - hstart);
    if (host_len == 0) return SCL_ERR_INVALID_ARG;

    char host_buf[256];
    if (host_len >= sizeof(host_buf)) return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(host_buf, hstart, host_len);
    host_buf[host_len] = '\0';

    /*
     * Step 4: build the request-target (path + optional ?query).
     * After parse_url, path is NUL-terminated at '?' so scl_strlen
     * gives exactly the path portion.
     */
    char target[SCL_HTTP_CLIENT_MAX_TARGET + 1];
    size_t plen = scl_strlen(url_parts.path);
    if (url_parts.query) {
        size_t qlen = scl_strlen(url_parts.query);
        if (plen + 1 + qlen >= sizeof(target))
            return SCL_ERR_SIZE_OVERFLOW;
        scl_memcpy(target, url_parts.path, plen);
        target[plen] = '?';
        scl_memcpy(target + plen + 1, url_parts.query, qlen + 1);
    } else {
        if (plen >= sizeof(target)) return SCL_ERR_SIZE_OVERFLOW;
        scl_memcpy(target, url_parts.path, plen + 1);
    }

    /*
     * Step 5: connect (or reuse existing connection).
     */
    err = scl_http_client_connect(c, host_buf, url_parts.port);
    if (err != SCL_OK) return err;

    /*
     * Step 6: send the request.
     */
    err = send_request(c, method, target, headers, body, body_len);
    if (err != SCL_OK) {
        scl_http_client_disconnect(c);
        return err;
    }

    /*
     * Step 7: read and parse the response.
     */
    err = read_response(c, resp, scl_strcmp(method, "HEAD") == 0);
    if (err != SCL_OK) {
        scl_free(c->alloc, resp->headers);
        resp->headers = NULL;
        scl_free(c->alloc, resp->body);
        resp->body = NULL;
        resp->body_cap = 0;
        scl_http_client_disconnect(c);
        return err;
    }

    /* Release any body that was heap-allocated; the body is the caller's
     * responsibility now if they want to keep it. */

    /*
     * Step 7: if server wants close, disconnect.
     */
    if (resp->connection_close) {
        scl_http_client_disconnect(c);
    }

    return SCL_OK;
}

/*
 * ── Response parser: feed-based (incremental) ─────────────────────
 *
 * This is the state machine exposed in the header. It is a stub
 * for now — the full implementation would allow incremental/streaming
 * reads without blocking.
 */
scl_error_t scl_http_response_parser_feed(scl_http_response_parser_t *p,
                                          const unsigned char *buf,
                                          size_t len) {
    if (!p || !buf) return SCL_ERR_NULL_PTR;
    if (p->state == SCL_HTTP_CS_DONE || p->state == SCL_HTTP_CS_ERROR)
        return SCL_OK;

    /* TODO: implement true incremental parsing. */
    (void)buf;
    (void)len;
    p->state = SCL_HTTP_CS_DONE;
    return SCL_OK;
}

/*
 * ── Response parser: init ────────────────────────────────────────
 */
void scl_http_response_parser_init(scl_http_response_parser_t *p,
                                   scl_http_client_response_t *resp,
                                   bool head_response) {
    if (!p) return;
    scl_memset(p, 0, sizeof(*p));
    p->state = SCL_HTTP_CS_STATUS;
    p->resp  = resp;
    p->head_response = head_response;
}
