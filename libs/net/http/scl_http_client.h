/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* HTTP/1.1 client. Keep-alive connection reuse, bounded response buffering,
 * percent-decoding, redirect following. */

#ifndef SCL_HTTP_CLIENT_H
#define SCL_HTTP_CLIENT_H

/*
 * scl_http_client — a security-first HTTP/1.1 client.
 *
 * ── Why a separate HTTP client? ──────────────────────────────────────────────
 *
 * The existing test for scl_http_server (tests/test_scl_http_server.c) embeds a
 * tiny inline HTTP client to drive the server. That pattern works for one-off
 * tests, but a proper library needs a reusable, well-tested client that:
 *
 *   1. Parses URLs safely (scheme, host, port, path)
 *   2. Manages TCP connections (connect / disconnect / keep-alive)
 *   3. Builds correctly-formed HTTP/1.1 request lines
 *   4. Validates the response stream before copying (no buffer overflows)
 *   5. Handles Transfer-Encoding, Content-Length, and Connection: close
 *   6. Respects the same security invariants the server does
 *
 * ── Security design ──────────────────────────────────────────────────────────
 *
 * The biggest risk in an HTTP client is parsing an attacker-controlled
 * response: a rogue or compromised server can send arbitrarily large or
 * malformed headers, lie about Content-Length, or attempt response-splitting.
 * We defend against these by:
 *
 *   • Bounding every buffer (headers, body, scratch) before parsing.
 *   • Using scl_range_in_bounds() and scl_clamp_len() on every offset/length
 *     derived from the response — exactly the hardening applied across the
 *     docparse parsers (test_scl_docparse_hardening.c validates this pattern).
 *   • Rejecting malformed status lines, negative Content-Length, chunked
 *     encoding we cannot safely de-chunk, and header lines missing ':',
 *   • Zeroing sensitive response data on client teardown (scl_secure_zero).
 *   • NULL-pointer guards on every string argument (project convention in
 *     scl_stdlib.h, scl_string.h).
 *
 * ── Keep-alive ───────────────────────────────────────────────────────────────
 *
 * By default the client follows the server's Connection header. If the server
 * sends "Connection: close" or the HTTP version is 1.0, we close after one
 * response. Otherwise we reuse the connection for the next request on the same
 * client handle (matching the server's keep-alive behaviour in
 * scl_http_server.c:handle_connection).
 *
 * ── Thread safety ────────────────────────────────────────────────────────────
 *
 * This client is NOT thread-safe. Each thread should own its own
 * scl_http_client_t (or protect shared access with a mutex). The underlying
 * scl_tcp_send_all / recv syscalls are thread-safe in the kernel, but the
 * internal buffer state is not.
 */

#include "scl_common.h" /* scl_error_t, scl_allocator_t, overflow-safe math */
#include "scl_string.h" /* scl_strlen, scl_strncpy, scl_strstr, etc. */

#include "scl_stdbool.h"
#include "scl_stddef.h"
#include "scl_stdint.h"

/* ── Limits (match the server's expectations) ─────────────────────
 * We impose the same bounds the server uses so that a request we send
 * will never be summarily rejected by a 414 / 431 from our own server. */
#define SCL_HTTP_CLIENT_MAX_TARGET 8190 /* same as SCL_HTTP_MAX_TARGET */
#define SCL_HTTP_CLIENT_MAX_HEADERS 64  /* same as SCL_HTTP_MAX_HEADERS */
#define SCL_HTTP_CLIENT_MAX_HEADER_BUF                                         \
  (64 * 1024) /* 64 KB for response headers */
#define SCL_HTTP_CLIENT_MAX_BODY_BUF                                           \
  (4 * 1024 * 1024) /* 4 MB default max body */
#define SCL_HTTP_CLIENT_DEFAULT_TIMEOUT_MS 15000

/* ── Parsed HTTP response ─────────────────────────────────────────
 *
 * This is what scl_http_client_request() fills in. The body pointer
 * lives in the client's internal buffer and is only valid until the
 * next request on the same client (or until scl_http_client_destroy).
 *
 * Design note: We store headers as a flat owned buffer with NUL-
 * separated lines rather than an array of parsed {name,value} pairs.
 * This avoids dynamic allocation during parsing and keeps the API
 * simple. The caller uses scl_http_client_find_header().
 *
 * NOTE: The name differs from the server's internal response type
 * (scl_http_response_t in scl_http_server.h) to avoid type-redefinition
 * errors when both headers are included in the same translation unit
 * (like the integration test). */
typedef struct {
  int status;           /* e.g. 200, 404, 500 */
  char status_text[64]; /* e.g. "OK", "Not Found" */

  char *headers;       /* owned: contiguous "Name: Value\0..." */
  size_t headers_len;  /* total bytes in headers (incl. NULs) */
  size_t header_count; /* number of header lines */

  void *body;      /* owned: response body bytes */
  size_t body_len; /* actual body length */
  size_t body_cap; /* allocated capacity of body buffer */

  /*
   * Whether the server wants the connection closed after this
   * response. The client checks "Connection: close" and HTTP/1.0
   * to set this, mirroring the server's logic in parse_request().
   */
  bool connection_close;
} scl_http_client_response_t;

/* ── HTTP client handle (opaque) ────────────────────────────────── */
typedef struct scl_http_client scl_http_client_t;

/*
 * ── Parse a URL into its components ──────────────────────────────
 *
 * Given a URL like "http://host:8080/path?query" or just "host:port/path",
 * this helper splits it safely into its parts. It does NOT perform a DNS
 * lookup or connect — it is purely a string parser.
 *
 * Security note: We reject URLs with embedded NUL bytes, control
 * characters in the host, or encoded credentials (the "user:pass@"
 * syntax) since the latter is a common phishing / SSRF vector.
 *
 * Output pointers point into the original URL string (no allocation).
 * Set a pointer to NULL if you do not need that component.
 *
 * Returns SCL_OK on success, or an error code on malformed input. */
typedef struct {
  const char *scheme; /* "http" or NULL (default "http") */
  const char *host;   /* hostname or IP, never NULL on success */
  uint16_t port;      /* parsed port, or 0 if default (80) */
  const char *path;   /* "/path" including leading '/', or "/" */
  const char *query;  /* query string after '?', or NULL */
} scl_http_url_t;

scl_error_t scl_http_parse_url(char *url_str, scl_http_url_t *out);

/*
 * ── Client lifecycle ─────────────────────────────────────────────
 *
 * Usage:
 *   1. scl_http_client_init(&client, alloc, max_body_size)
 *   2. scl_http_client_request(client, "GET", "http://host/path",
 *                              headers_str, body, body_len, &response)
 *   3. ... use the response ...
 *   4. scl_http_client_destroy(client)
 *
 * For many requests to the same host, the client reuses the TCP
 * connection (keep-alive). Call scl_http_client_disconnect() to
 * force-close if you know you are done. */

/* Allocate and initialise a client. max_body_size caps the body we
 * will accept (prevents memory exhaustion from a lying peer). */
scl_error_t scl_http_client_init(scl_allocator_t *alloc,
                                 scl_http_client_t **out, size_t max_body_size);

/* Free all resources (closes any open connection). */
void scl_http_client_destroy(scl_http_client_t *c);

/* Convenience: free the response body and headers allocated by a previous
 * call to scl_http_client_request(). This lets callers keep responses
 * past the next request without leaking. Safe to call with a zeroed-out
 * response (checks for NULL pointers). After calling this, the response
 * struct should be re-initialised before reuse (scl_memset to 0). */
void scl_http_client_request_free(scl_allocator_t *alloc,
                                  scl_http_client_response_t *resp);

/* Set receive timeout (ms) for the current connection. 0 = no timeout.
 * Affects subsequent recv() calls. The default is
 * SCL_HTTP_CLIENT_DEFAULT_TIMEOUT_MS (15000). */
void scl_http_client_set_timeout(scl_http_client_t *c, int64_t timeout_ms);

/* ── Connection management ────────────────────────────────────────
 *
 * Normally you do not call these directly — request() calls connect()
 * automatically when needed. Call disconnect() explicitly if you want
 * to force a new TCP connection for the next request. */

/* Connect to host:port. If already connected to the same host:port
 * and keep-alive is viable, this is a no-op (returns SCL_OK). */
scl_error_t scl_http_client_connect(scl_http_client_t *c, const char *host,
                                    uint16_t port);

/* Disconnect (close TCP socket). Idempotent. */
void scl_http_client_disconnect(scl_http_client_t *c);

/* ── Request / response ───────────────────────────────────────────
 *
 * Send an HTTP request and read the full response. This is the main
 * entry point. It:
 *   1. Parses the URL to extract host, port, path
 *   2. Connects (or reuses existing connection)
 *   3. Builds and sends the request line + headers
 *   4. Sends the request body (if any)
 *   5. Reads and parses the response
 *
 * `method`  — e.g. "GET", "POST", "HEAD", "PUT", "DELETE"
 * `url`     — full URL like "http://host:port/path"
 * `headers` — extra headers as a single string, e.g.
 *             "Content-Type: application/json\r\nX-Custom: value\r\n"
 *             May be NULL or empty.
 * `body`    — request body bytes (may be NULL for GET/HEAD)
 * `body_len`— length of request body
 * `resp`    — filled with parsed response (valid until next request
 *             or destroy)
 *
 * Returns SCL_OK on success, or an error code on network / parse
 * failure. On error the response may be partially filled. */
scl_error_t scl_http_client_request(scl_http_client_t *c, const char *method,
                                    const char *url, const char *headers,
                                    const void *body, size_t body_len,
                                    scl_http_client_response_t *resp);

/* Convenience wrappers. */
static inline scl_error_t
scl_http_client_get(scl_http_client_t *c, const char *url, const char *headers,
                    scl_http_client_response_t *resp) {
  return scl_http_client_request(c, "GET", url, headers, NULL, 0, resp);
}

static inline scl_error_t
scl_http_client_post(scl_http_client_t *c, const char *url, const char *headers,
                     const void *body, size_t body_len,
                     scl_http_client_response_t *resp) {
  return scl_http_client_request(c, "POST", url, headers, body, body_len, resp);
}

static inline scl_error_t
scl_http_client_head(scl_http_client_t *c, const char *url, const char *headers,
                     scl_http_client_response_t *resp) {
  return scl_http_client_request(c, "HEAD", url, headers, NULL, 0, resp);
}

/* ── Response inspection ──────────────────────────────────────────
 *
 * Find a header value by name (case-insensitive). Returns NULL if not
 * found. The returned pointer is valid until the next request. */
const char *scl_http_client_find_header(const scl_http_client_response_t *resp,
                                        const char *name);

/*
 * ── Internal: response reader state machine ──────────────────────
 *
 * Exposed only so the test can exercise the parser in isolation.
 * Normal callers should use scl_http_client_request(). */

/* Parser states for the response-reading state machine. We track
 * which part of the response we are in so that a partial read from
 * the kernel resumes where we left off, not from the beginning. */
typedef enum {
  SCL_HTTP_CS_STATUS,     /* reading "HTTP/1.1 200 OK\r\n" */
  SCL_HTTP_CS_HEADERS,    /* reading header lines til blank line */
  SCL_HTTP_CS_BODY_CL,    /* reading body by Content-Length */
  SCL_HTTP_CS_BODY_CLOSE, /* reading body until peer closes */
  SCL_HTTP_CS_CHUNK_SIZE, /* reading chunk size line */
  SCL_HTTP_CS_CHUNK_DATA, /* reading chunk data */
  SCL_HTTP_CS_CHUNK_TRAILER, /* reading chunk trailing \r\n */
  SCL_HTTP_CS_DONE,       /* response fully read */
  SCL_HTTP_CS_ERROR       /* irrecoverable parse error */
} scl_http_client_state_t;

/* Internal buffer for incremental parsing. */
typedef struct {
  unsigned char *data;
  size_t len;
  size_t cap;
} scl_http__ibuf_t;

/* Low-level: feed raw bytes into the response parser. */
typedef struct {
  scl_http_client_state_t state;
  scl_http_client_response_t *resp;
  scl_allocator_t *alloc;
  size_t max_body_size;    /* cap from client for body buffering */
  size_t content_length;   /* from Content-Length header (or 0) */
  size_t body_read;        /* bytes of body accumulated so far */
  bool head_response;      /* true if this was a HEAD request */
  bool chunked;            /* true if Transfer-Encoding: chunked */
  size_t chunk_remaining;  /* bytes left in current chunk */
  scl_http__ibuf_t ibuf;   /* internal accumulator */
} scl_http_response_parser_t;

void scl_http_response_parser_init(scl_http_response_parser_t *p,
                                    scl_http_client_response_t *resp,
                                    scl_allocator_t *alloc,
                                    size_t max_body_size,
                                    bool head_response);

/* Feed len bytes from buf into the parser. Returns SCL_OK on
 * progress, SCL_ERR_PARSE on malformed data, SCL_ERR_SIZE_OVERFLOW
 * if the body exceeds limits. When state == SCL_HTTP_CS_DONE the
 * response is complete. */
scl_error_t scl_http_response_parser_feed(scl_http_response_parser_t *p,
                                          const unsigned char *buf, size_t len);

#endif /* SCL_HTTP_CLIENT_H */
