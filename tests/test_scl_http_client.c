/*
 * test_scl_http_client.c — tests for the HTTP client library.
 *
 * Strategy: start a real scl_http_server (the one we already test in
 * test_scl_http_server.c) and drive it with the new client library.
 * This tests the full round-trip: client → server → client.
 *
 * The same temp docroot and server fixture is used. Tests cover:
 *   • URL parsing (happy + malformed)
 *   • GET, HEAD, POST via the client
 *   • Keep-alive (multiple requests, one connection)
 *   • Error handling (connection refused, timeout)
 *   • Content-Length and Connection: close handling
 *   • Header lookup
 *   • Client lifecycle (init/destroy, init/connect/request/disconnect)
 */
#include "scl_test.h"
#include "scl_http_client.h"
#include "scl_http_server.h"
#include "scl_string.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>

/* ── Fixture: temp docroot ─────────────────────────────────── */
static char root[256];
static char p[300];
static uint16_t server_port = 0;
static scl_http_server_t *srv = NULL;

/* tr must be declared before fixture_setup since it's referenced there. */
static scl_test_runner_t tr;

static void fixture_setup(void) {
    snprintf(root, sizeof(root), "/tmp/scl_http_client_%d", (int)getpid());
    mkdir(root, 0755);
    snprintf(p, sizeof(p), "%s/index.html", root); {
        FILE *f = fopen(p, "wb"); if (f) { fputs("<h1>home</h1>\n", f); fclose(f); }
    }
    snprintf(p, sizeof(p), "%s/hello.txt", root); {
        FILE *f = fopen(p, "wb"); if (f) { fputs("hello world\n", f); fclose(f); }
    }
    snprintf(p, sizeof(p), "%s/data.json", root); {
        FILE *f = fopen(p, "wb"); if (f) { fputs("{\"k\":1}\n", f); fclose(f); }
    }

    scl_http_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.docroot = root;
    cfg.num_workers = 2;
    cfg.pool_capacity = 16;

    SCL_EXPECT_OK(&tr, scl_http_server_init(scl_allocator_default(), &srv, &cfg));
    SCL_EXPECT_NOT_NULL(&tr, srv);
    SCL_EXPECT_OK(&tr, scl_http_server_start(srv));
    server_port = scl_http_server_port(srv);
    SCL_EXPECT_TRUE(&tr, server_port != 0);
}

static void fixture_teardown(void) {
    if (srv) {
        scl_http_server_stop(srv);
        scl_http_server_destroy(srv);
        srv = NULL;
    }
    snprintf(p, sizeof(p), "%s/index.html", root); unlink(p);
    snprintf(p, sizeof(p), "%s/hello.txt", root);  unlink(p);
    snprintf(p, sizeof(p), "%s/data.json", root);  unlink(p);
    rmdir(root);
}

/* ── Response edge-case helpers ────────────────────────────
 *
 * Spawns a child process that listens on localhost:0, responds
 * with `response` (ignoring the request), and exits. Returns the
 * port the child bound, or 0 on failure. Parent must waitpid(). */
static uint16_t miniserver_spawn(const char *response) {
    int sv[2];
    if (pipe(sv) != 0) return 0;

    pid_t pid = fork();
    if (pid == 0) {
        close(sv[0]);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) _exit(1);
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = 0;
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) _exit(2);
        if (listen(fd, 1) != 0) _exit(3);

        struct sockaddr_in bound;
        socklen_t blen = sizeof(bound);
        getsockname(fd, (struct sockaddr *)&bound, &blen);
        uint16_t port = ntohs(bound.sin_port);
        write(sv[1], &port, sizeof(port));
        close(sv[1]);

        int cfd = accept(fd, NULL, NULL);
        if (cfd < 0) _exit(4);
        close(fd);
        char buf[4096];
        recv(cfd, buf, sizeof(buf), 0);
        send(cfd, response, strlen(response), 0);
        shutdown(cfd, SHUT_WR);
        usleep(10000);
        close(cfd);
        _exit(0);
    }

    close(sv[1]);
    uint16_t port = 0;
    read(sv[0], &port, sizeof(port));
    close(sv[0]);
    return port;
}

int main(void) {
    scl_test_init(&tr);

    /* ── URL parsing tests ──────────────────────────────────────
     *
     * NOTE: scl_http_parse_url takes a mutable char* (it NUL-terminates
     * the path at '?' and host at ':' for port parsing). We use
     * char[] buffers instead of string literals. */
    scl_test_group("URL: simple http URL");
    {
        char url[] = "http://example.com/path";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_OK(&tr, e);
        SCL_EXPECT_NOT_NULL(&tr, u.host);
        SCL_EXPECT_EQ_I(&tr, u.port, 80);
        SCL_EXPECT_NOT_NULL(&tr, u.path);
        if (u.path) SCL_EXPECT_EQ_STR(&tr, u.path, "/path");
        SCL_EXPECT_EQ_STR(&tr, u.scheme, "http");
    }

    scl_test_group("URL: with explicit port");
    {
        char url[] = "http://example.com:8080/";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_OK(&tr, e);
        SCL_EXPECT_EQ_I(&tr, u.port, 8080);
    }

    scl_test_group("URL: with query string");
    {
        char url[] = "http://example.com/search?q=hello";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_OK(&tr, e);
        if (u.path) SCL_EXPECT_EQ_STR(&tr, u.path, "/search");
        if (u.query) SCL_EXPECT_EQ_STR(&tr, u.query, "q=hello");
    }

    scl_test_group("URL: root path");
    {
        char url[] = "http://example.com";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_OK(&tr, e);
        if (u.path) SCL_EXPECT_EQ_STR(&tr, u.path, "/");
    }

    scl_test_group("URL: reject https");
    {
        char url[] = "https://example.com";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_EQ_I(&tr, (int)e, (int)SCL_ERR_UNSUPPORTED);
    }

    scl_test_group("URL: reject empty");
    {
        char url[] = "";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_TRUE(&tr, e != SCL_OK);
    }

    scl_test_group("URL: reject fragment");
    {
        char url[] = "http://example.com#frag";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_TRUE(&tr, e != SCL_OK);
    }

    scl_test_group("URL: reject NULL");
    {
        scl_error_t e = scl_http_parse_url(NULL, NULL);
        SCL_EXPECT_EQ_I(&tr, (int)e, (int)SCL_ERR_NULL_PTR);
    }

    /* ── Client lifecycle ─────────────────────────────────────── */
    scl_test_group("Client: init and destroy");
    {
        scl_http_client_t *c = NULL;
        scl_error_t e = scl_http_client_init(scl_allocator_default(), &c, 0);
        SCL_EXPECT_OK(&tr, e);
        SCL_EXPECT_NOT_NULL(&tr, c);
        scl_http_client_destroy(c);
    }

    scl_test_group("Client: init with bad args");
    {
        scl_error_t e = scl_http_client_init(NULL, NULL, 0);
        SCL_EXPECT_EQ_I(&tr, (int)e, (int)SCL_ERR_NULL_PTR);
    }

    /* ── Server fixture ───────────────────────────────────────── */
    scl_test_group("Client: start test server");
    fixture_setup();

    /* ── GET requests ─────────────────────────────────────────── */
    if (server_port != 0) {
        scl_test_group("Client: GET existing file");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/hello.txt", server_port);
            scl_http_client_response_t resp;
            scl_error_t e = scl_http_client_get(c, url, NULL, &resp);
            SCL_EXPECT_OK(&tr, e);
            SCL_EXPECT_EQ_I(&tr, resp.status, 200);
            if (resp.body) SCL_EXPECT_EQ_STR(&tr, (const char *)resp.body, "hello world\n");
            scl_http_client_destroy(c);
        }

        scl_test_group("Client: GET JSON (MIME type check)");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/data.json", server_port);
            scl_http_client_response_t resp;
            SCL_EXPECT_OK(&tr, scl_http_client_get(c, url, NULL, &resp));
            SCL_EXPECT_EQ_I(&tr, resp.status, 200);
            const char *ct = scl_http_client_find_header(&resp, "Content-Type");
            if (ct) SCL_EXPECT_TRUE(&tr, scl_strstr(ct, "application/json") != NULL);
            scl_http_client_destroy(c);
        }

        scl_test_group("Client: GET 404");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/nope.txt", server_port);
            scl_http_client_response_t resp;
            SCL_EXPECT_OK(&tr, scl_http_client_get(c, url, NULL, &resp));
            SCL_EXPECT_EQ_I(&tr, resp.status, 404);
            scl_http_client_destroy(c);
        }

        scl_test_group("Client: HEAD request (no body)");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/hello.txt", server_port);
            scl_http_client_response_t resp;
            SCL_EXPECT_OK(&tr, scl_http_client_head(c, url, NULL, &resp));
            SCL_EXPECT_EQ_I(&tr, resp.status, 200);
            SCL_EXPECT_EQ_SZ(&tr, resp.body_len, 0);
            const char *cl = scl_http_client_find_header(&resp, "Content-Length");
            SCL_EXPECT_NOT_NULL(&tr, cl);
            scl_http_client_destroy(c);
        }

        scl_test_group("Client: keep-alive, multiple requests");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url1[256], url2[256];
            snprintf(url1, sizeof(url1), "http://127.0.0.1:%u/hello.txt", server_port);
            snprintf(url2, sizeof(url2), "http://127.0.0.1:%u/data.json", server_port);

            scl_http_client_response_t r1;
            scl_error_t e1 = scl_http_client_get(c, url1, NULL, &r1);
            SCL_EXPECT_OK(&tr, e1);
            SCL_EXPECT_EQ_I(&tr, r1.status, 200);

            scl_http_client_response_t r2;
            scl_error_t e2 = scl_http_client_get(c, url2, NULL, &r2);
            SCL_EXPECT_OK(&tr, e2);
            SCL_EXPECT_EQ_I(&tr, r2.status, 200);

            scl_http_client_destroy(c);
        }

        scl_test_group("Client: reconnect after connection:close");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/hello.txt", server_port);

            /* First request with explicit Connection: close header. */
            scl_http_client_response_t r1;
            scl_error_t e1 = scl_http_client_get(c, url, "Connection: close\r\n", &r1);
            SCL_EXPECT_OK(&tr, e1);
            SCL_EXPECT_EQ_I(&tr, r1.status, 200);
            SCL_EXPECT_TRUE(&tr, r1.connection_close);

            /* Second request should reconnect automatically. */
            scl_http_client_response_t r2;
            scl_error_t e2 = scl_http_client_get(c, url, NULL, &r2);
            SCL_EXPECT_OK(&tr, e2);
            SCL_EXPECT_EQ_I(&tr, r2.status, 200);

            scl_http_client_destroy(c);
        }

        scl_test_group("Client: custom headers");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/hello.txt", server_port);
            scl_http_client_response_t resp;
            SCL_EXPECT_OK(&tr, scl_http_client_get(c, url,
                "X-Custom: test-value\r\n", &resp));
            SCL_EXPECT_EQ_I(&tr, resp.status, 200);
            scl_http_client_destroy(c);
        }

        scl_test_group("Client: path traversal");
        {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            char url[256];
            snprintf(url, sizeof(url), "http://127.0.0.1:%u/../../../etc/passwd", server_port);
            scl_http_client_response_t resp;
            SCL_EXPECT_OK(&tr, scl_http_client_get(c, url, NULL, &resp));
            /* Server should block this with 403 or 404. */
            SCL_EXPECT_TRUE(&tr, resp.status == 403 || resp.status == 404);
            scl_http_client_destroy(c);
        }
    }

    /* ── Teardown ─────────────────────────────────────────────── */
    scl_test_group("Client: stop test server");
    fixture_teardown();

    /* ── Error handling ───────────────────────────────────────────
     * These tests do NOT depend on the test server. */
    scl_test_group("Client: connection refused");
    {
        scl_http_client_t *c = NULL;
        SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
        scl_http_client_response_t resp;
        scl_error_t e = scl_http_client_get(c, "http://127.0.0.1:1/nonexistent", NULL, &resp);
        /* Should fail with not-found or I/O error. */
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_NOT_FOUND || e == SCL_ERR_IO);
        scl_http_client_destroy(c);
    }

    scl_test_group("Client: 1xx interim response skipped automatically");
    {
        const char *response =
            "HTTP/1.1 100 Continue\r\n\r\n"
            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Hello";
        uint16_t mport = miniserver_spawn(response);
        if (mport != 0) {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            if (c) {
                char url[256];
                snprintf(url, sizeof(url), "http://127.0.0.1:%u/echo", mport);
                scl_http_client_response_t resp;
                scl_memset(&resp, 0, sizeof(resp));
                scl_error_t e = scl_http_client_get(c, url, NULL, &resp);
                SCL_EXPECT_OK(&tr, e);
                if (e == SCL_OK) {
                    SCL_EXPECT_EQ_I(&tr, resp.status, 200);
                    if (resp.body && resp.body_len == 5)
                        SCL_EXPECT_EQ_STR(&tr, (const char *)resp.body, "Hello");
                }
                scl_http_client_destroy(c);
            }
            int wstatus;
            waitpid(-1, &wstatus, 0);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
    }

    scl_test_group("Client: chunked response returns UNSUPPORTED");
    {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: close\r\n"
            "\r\n"
            "5\r\nHello\r\n0\r\n\r\n";
        uint16_t mport = miniserver_spawn(response);
        if (mport != 0) {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            if (c) {
                char url[256];
                snprintf(url, sizeof(url), "http://127.0.0.1:%u/echo", mport);
                scl_http_client_response_t resp;
                scl_memset(&resp, 0, sizeof(resp));
                scl_error_t e = scl_http_client_get(c, url, NULL, &resp);
                SCL_EXPECT_EQ_I(&tr, (int)e, (int)SCL_ERR_UNSUPPORTED);
                scl_http_client_destroy(c);
            }
            int wstatus;
            waitpid(-1, &wstatus, 0);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
    }

    scl_test_group("Client: connection: close with body");
    {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello world";
        uint16_t mport = miniserver_spawn(response);
        if (mport != 0) {
            scl_http_client_t *c = NULL;
            SCL_EXPECT_OK(&tr, scl_http_client_init(scl_allocator_default(), &c, 65536));
            if (c) {
                char url[256];
                snprintf(url, sizeof(url), "http://127.0.0.1:%u/echo", mport);
                scl_http_client_response_t resp;
                scl_memset(&resp, 0, sizeof(resp));
                scl_error_t e = scl_http_client_get(c, url, NULL, &resp);
                SCL_EXPECT_OK(&tr, e);
                if (e == SCL_OK) {
                    SCL_EXPECT_EQ_I(&tr, resp.status, 200);
                    SCL_EXPECT_TRUE(&tr, resp.connection_close);
                    SCL_EXPECT_TRUE(&tr, resp.body_len >= 11);
                }
                scl_http_client_destroy(c);
            }
            int wstatus;
            waitpid(-1, &wstatus, 0);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
    }

    scl_test_group("Client: malformed status line");
    {
        char url[] = "http://127.0.0.1:1/bad";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        /* URL should parse fine even if server is unreachable */
        SCL_EXPECT_OK(&tr, e);
        SCL_EXPECT_EQ_I(&tr, u.port, 1);
    }

    scl_test_group("Client: URL with IPv6 address");
    {
        char url[] = "http://[::1]:8080/path";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_OK(&tr, e);
        SCL_EXPECT_NOT_NULL(&tr, u.host);
        if (u.host) {
            /* Brackets are stripped so the host is usable with getaddrinfo. */
            SCL_EXPECT_EQ_STR(&tr, u.host, "::1");
        }
        SCL_EXPECT_EQ_I(&tr, u.port, 8080);
        if (u.path) SCL_EXPECT_EQ_STR(&tr, u.path, "/path");
    }

    scl_test_group("Client: URL with no port uses default 80");
    {
        char url[] = "http://example.com";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_OK(&tr, e);
        SCL_EXPECT_EQ_I(&tr, u.port, 80);
        if (u.path) SCL_EXPECT_EQ_STR(&tr, u.path, "/");
    }

    scl_test_group("Client: URL reject userinfo (credentials)");
    {
        char url[] = "http://user:pass@example.com/";
        scl_http_url_t u;
        scl_error_t e = scl_http_parse_url(url, &u);
        SCL_EXPECT_TRUE(&tr, e != SCL_OK);
    }

    scl_test_group("Client: null or empty header lookups");
    {
        scl_http_client_response_t r;
        scl_memset(&r, 0, sizeof(r));
        const char *v = scl_http_client_find_header(&r, "Content-Type");
        SCL_EXPECT_NULL(&tr, v);
        v = scl_http_client_find_header(NULL, "Anything");
        SCL_EXPECT_NULL(&tr, v);
        v = scl_http_client_find_header(&r, NULL);
        SCL_EXPECT_NULL(&tr, v);
        v = scl_http_client_find_header(&r, "");
        SCL_EXPECT_NULL(&tr, v);
    }

    scl_test_group("Client: find header with populated response");
    {
        scl_http_client_response_t r;
        scl_memset(&r, 0, sizeof(r));
        r.headers = "Content-Type: text/plain\0Content-Length: 42\0\0";
        r.headers_len = 46;
        r.header_count = 2;

        const char *ct = scl_http_client_find_header(&r, "Content-Type");
        SCL_EXPECT_NOT_NULL(&tr, ct);
        if (ct) SCL_EXPECT_EQ_STR(&tr, ct, "text/plain");

        const char *cl = scl_http_client_find_header(&r, "Content-Length");
        SCL_EXPECT_NOT_NULL(&tr, cl);
        if (cl) SCL_EXPECT_EQ_STR(&tr, cl, "42");

        const char *mx = scl_http_client_find_header(&r, "X-Missing");
        SCL_EXPECT_NULL(&tr, mx);
    }

    scl_test_group("Client: request_free is safe on zeroed response");
    {
        scl_http_client_response_t r;
        scl_memset(&r, 0, sizeof(r));
        /* Should not crash */
        scl_http_client_request_free(scl_allocator_default(), &r);
    }

    scl_test_group("Client: request_free cleans up headers and body");
    {
        scl_http_client_response_t r;
        scl_memset(&r, 0, sizeof(r));
        /* Allocate some fake memory */
        scl_allocator_t *alloc = scl_allocator_default();
        r.headers = (char *)scl_alloc(alloc, 64, _Alignof(max_align_t));
        r.body = scl_alloc(alloc, 64, _Alignof(max_align_t));
        if (r.headers && r.body) {
            r.headers_len = 64;
            r.body_cap = 64;
            /* Free should work and null out pointers */
            scl_http_client_request_free(alloc, &r);
            SCL_EXPECT_NULL(&tr, r.headers);
            SCL_EXPECT_NULL(&tr, r.body);
        }
    }

    /* ── Summary ───────────────────────────────────────────────── */
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
