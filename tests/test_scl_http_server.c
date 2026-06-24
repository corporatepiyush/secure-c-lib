/* End-to-end test for scl_http_server: starts a real server on a loopback
 * port and drives it with real TCP requests, asserting the security-critical
 * behaviors (path-traversal defense, method/version handling, framing) plus
 * keep-alive, MIME typing, and the dynamic handler hook. */
#include "scl_test.h"
#include "scl_http_server.h"
#include "scl_string.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>

/* ── Tiny blocking HTTP client ─────────────────────────────── */
typedef struct {
    int    status;
    char   headers[8192];
    size_t header_len;
    char   body[65536];
    size_t body_len;
} resp_t;

static int client_connect(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(fd); return -1; }
    return fd;
}

static long find_content_length(const char *headers) {
    const char *p = headers;
    while (*p) {
        if (strncasecmp(p, "Content-Length:", 15) == 0) {
            p += 15;
            while (*p == ' ') p++;
            return strtol(p, NULL, 10);
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return -1;
}

/* Read exactly one HTTP response off `fd`. is_head suppresses body reads. */
static int read_response(int fd, resp_t *r, int is_head) {
    memset(r, 0, sizeof(*r));
    char buf[8192];
    size_t total = 0;
    char *hdr_end = NULL;

    /* Accumulate until end of headers. */
    while (!hdr_end && total < sizeof(buf) - 1) {
        ssize_t n = recv(fd, buf + total, sizeof(buf) - 1 - total, 0);
        if (n <= 0) return -1;
        total += (size_t)n;
        buf[total] = '\0';
        hdr_end = strstr(buf, "\r\n\r\n");
    }
    if (!hdr_end) return -1;

    size_t hlen = (size_t)(hdr_end - buf) + 4;
    if (hlen >= sizeof(r->headers)) return -1;
    memcpy(r->headers, buf, hlen);
    r->headers[hlen] = '\0';
    r->header_len = hlen;

    if (sscanf(buf, "HTTP/1.%*d %d", &r->status) != 1) return -1;

    long clen = find_content_length(r->headers);
    if (is_head) { r->body_len = 0; return 0; }
    if (clen < 0) clen = 0;
    if ((size_t)clen >= sizeof(r->body)) return -1;

    size_t have = total - hlen;
    if (have > (size_t)clen) have = (size_t)clen;
    memcpy(r->body, buf + hlen, have);
    r->body_len = have;
    while (r->body_len < (size_t)clen) {
        ssize_t n = recv(fd, r->body + r->body_len, (size_t)clen - r->body_len, 0);
        if (n <= 0) break;
        r->body_len += (size_t)n;
    }
    r->body[r->body_len] = '\0';
    return 0;
}

/* One request on a fresh connection. */
static int do_request(uint16_t port, const char *raw, resp_t *r, int is_head) {
    int fd = client_connect(port);
    if (fd < 0) return -1;
    if (send(fd, raw, strlen(raw), 0) < 0) { close(fd); return -1; }
    int rc = read_response(fd, r, is_head);
    close(fd);
    return rc;
}

/* ── Dynamic handler under test ────────────────────────────── */
static bool health_handler(const scl_http_request_t *req, scl_http_response_t *resp, void *user) {
    (void)user;
    if (scl_strcmp(req->path, "/health") == 0) {
        static const char json[] = "{\"status\":\"ok\"}";
        resp->status = 200;
        resp->content_type = "application/json";
        resp->body = json;
        resp->body_len = sizeof(json) - 1;
        return true;
    }
    return false;
}

/* ── Fixture: temp docroot ─────────────────────────────────── */
static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (f) { fputs(content, f); fclose(f); }
}

static int header_has(const resp_t *r, const char *substr) {
    return strstr(r->headers, substr) != NULL;
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    /* Build a temp docroot. */
    char root[256];
    snprintf(root, sizeof(root), "/tmp/scl_httpd_%d", (int)getpid());
    mkdir(root, 0755);
    char p[300];
    snprintf(p, sizeof(p), "%s/index.html", root); write_file(p, "<h1>home</h1>\n");
    snprintf(p, sizeof(p), "%s/hello.txt", root);  write_file(p, "hello world\n");
    snprintf(p, sizeof(p), "%s/data.json", root);  write_file(p, "{\"k\":1}\n");
    char subdir[300];
    snprintf(subdir, sizeof(subdir), "%s/sub", root); mkdir(subdir, 0755);
    snprintf(p, sizeof(p), "%s/sub/page.css", root); write_file(p, "body{}\n");

    scl_http_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.host = "127.0.0.1";
    cfg.port = 0;                 /* ephemeral */
    cfg.docroot = root;
    cfg.num_workers = 4;
    cfg.pool_capacity = 64;
    cfg.handler = health_handler;

    scl_http_server_t *srv = NULL;
    scl_test_group("HTTP: server starts");
    SCL_EXPECT_OK(&tr, scl_http_server_init(scl_allocator_default(), &srv, &cfg));
    SCL_EXPECT_NOT_NULL(&tr, srv);
    SCL_EXPECT_OK(&tr, scl_http_server_start(srv));
    uint16_t port = scl_http_server_port(srv);
    SCL_EXPECT_TRUE(&tr, port != 0);

    resp_t r;

    scl_test_group("HTTP: static file GET");
    if (do_request(port, "GET /hello.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_EQ_I(&tr, r.status, 200);
        SCL_EXPECT_EQ_STR(&tr, r.body, "hello world\n");
        SCL_EXPECT_TRUE(&tr, header_has(&r, "text/plain"));
    } else { SCL_EXPECT_TRUE(&tr, 0); }

    scl_test_group("HTTP: directory index");
    if (do_request(port, "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_EQ_I(&tr, r.status, 200);
        SCL_EXPECT_TRUE(&tr, strstr(r.body, "home") != NULL);
        SCL_EXPECT_TRUE(&tr, header_has(&r, "text/html"));
    } else { SCL_EXPECT_TRUE(&tr, 0); }

    scl_test_group("HTTP: MIME by extension");
    if (do_request(port, "GET /data.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_EQ_I(&tr, r.status, 200);
        SCL_EXPECT_TRUE(&tr, header_has(&r, "application/json"));
    } else { SCL_EXPECT_TRUE(&tr, 0); }
    if (do_request(port, "GET /sub/page.css HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_EQ_I(&tr, r.status, 200);
        SCL_EXPECT_TRUE(&tr, header_has(&r, "text/css"));
    } else { SCL_EXPECT_TRUE(&tr, 0); }

    scl_test_group("HTTP: 404 for missing file");
    if (do_request(port, "GET /nope.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 404);
    else SCL_EXPECT_TRUE(&tr, 0);

    scl_test_group("HTTP: path traversal is blocked");
    /* literal ../ */
    if (do_request(port, "GET /../../../etc/passwd HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_TRUE(&tr, r.status == 403 || r.status == 404);
        SCL_EXPECT_TRUE(&tr, strstr(r.body, "root:") == NULL);
    } else { SCL_EXPECT_TRUE(&tr, 0); }
    /* percent-encoded ../ */
    if (do_request(port, "GET /%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_TRUE(&tr, r.status == 403 || r.status == 404);
        SCL_EXPECT_TRUE(&tr, strstr(r.body, "root:") == NULL);
    } else { SCL_EXPECT_TRUE(&tr, 0); }

    scl_test_group("HTTP: encoded NUL rejected");
    if (do_request(port, "GET /hello.txt%00.png HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 400);
    else SCL_EXPECT_TRUE(&tr, 0);

    scl_test_group("HTTP: method not allowed / version");
    if (do_request(port, "POST /hello.txt HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 405);
    else SCL_EXPECT_TRUE(&tr, 0);
    if (do_request(port, "GET /hello.txt HTTP/2.0\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 505);
    else SCL_EXPECT_TRUE(&tr, 0);

    scl_test_group("HTTP: malformed request line -> 400");
    if (do_request(port, "GET\r\n\r\n", &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 400);
    else SCL_EXPECT_TRUE(&tr, 0);

    scl_test_group("HTTP: HEAD returns headers, no body");
    if (do_request(port, "HEAD /hello.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 1) == 0) {
        SCL_EXPECT_EQ_I(&tr, r.status, 200);
        SCL_EXPECT_EQ_SZ(&tr, r.body_len, 0);
        SCL_EXPECT_TRUE(&tr, header_has(&r, "Content-Length:"));
    } else { SCL_EXPECT_TRUE(&tr, 0); }

    scl_test_group("HTTP: dynamic handler");
    if (do_request(port, "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r, 0) == 0) {
        SCL_EXPECT_EQ_I(&tr, r.status, 200);
        SCL_EXPECT_EQ_STR(&tr, r.body, "{\"status\":\"ok\"}");
        SCL_EXPECT_TRUE(&tr, header_has(&r, "application/json"));
    } else { SCL_EXPECT_TRUE(&tr, 0); }

    scl_test_group("HTTP: keep-alive serves multiple requests");
    {
        int fd = client_connect(port);
        SCL_EXPECT_TRUE(&tr, fd >= 0);
        if (fd >= 0) {
            const char *req1 = "GET /hello.txt HTTP/1.1\r\nHost: x\r\n\r\n";
            const char *req2 = "GET /data.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
            send(fd, req1, strlen(req1), 0);
            int ok1 = read_response(fd, &r, 0);
            int s1 = r.status;
            send(fd, req2, strlen(req2), 0);
            int ok2 = read_response(fd, &r, 0);
            int s2 = r.status;
            SCL_EXPECT_EQ_I(&tr, ok1, 0);
            SCL_EXPECT_EQ_I(&tr, ok2, 0);
            SCL_EXPECT_EQ_I(&tr, s1, 200);
            SCL_EXPECT_EQ_I(&tr, s2, 200);
            close(fd);
        }
    }

    scl_test_group("HTTP: server stops cleanly");
    SCL_EXPECT_OK(&tr, scl_http_server_stop(srv));
    scl_http_server_destroy(srv);

    /* cleanup temp files */
    snprintf(p, sizeof(p), "%s/index.html", root); unlink(p);
    snprintf(p, sizeof(p), "%s/hello.txt", root);  unlink(p);
    snprintf(p, sizeof(p), "%s/data.json", root);  unlink(p);
    snprintf(p, sizeof(p), "%s/sub/page.css", root); unlink(p);
    snprintf(p, sizeof(p), "%s/sub", root); rmdir(p);
    rmdir(root);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
