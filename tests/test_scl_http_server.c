/* End-to-end test for scl_http_server: starts a real server on a loopback
 * port and drives it with real TCP requests, asserting the security-critical
 * behaviors (path-traversal defense, method/version handling, framing) plus
 * keep-alive, MIME typing, and the dynamic handler hook. */
#include "scl_http_server.h"
#include "scl_string.h"
#include "scl_test.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/* ── Tiny blocking HTTP client ─────────────────────────────── */
typedef struct {
  int status;
  char headers[8192];
  size_t header_len;
  char body[65536];
  size_t body_len;
} resp_t;

static int client_connect(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static long find_content_length(const char *headers) {
  const char *p = headers;
  while (*p) {
    if (strncasecmp(p, "Content-Length:", 15) == 0) {
      p += 15;
      while (*p == ' ')
        p++;
      return strtol(p, NULL, 10);
    }
    const char *nl = strchr(p, '\n');
    if (!nl)
      break;
    p = nl + 1;
  }
  return -1;
}

/* Read exactly one HTTP response off `fd`. is_head suppresses body reads.
 * Skips interim 1xx responses (RFC 7231 §6.2) so these are transparent
 * to the caller. */
static int read_response(int fd, resp_t *r, int is_head) {
  memset(r, 0, sizeof(*r));
  char buf[8192];
  size_t total = 0;

  for (;;) {
    /* Preserve any bytes already buffered from a prior 1xx skip — a
     * coalesced "100 Continue" + final response can arrive in one recv. */
    char *hdr_end = (total > 0) ? strstr(buf, "\r\n\r\n") : NULL;

    while (!hdr_end && total < sizeof(buf) - 1) {
      ssize_t n = recv(fd, buf + total, sizeof(buf) - 1 - total, 0);
      if (n <= 0)
        return -1;
      total += (size_t)n;
      buf[total] = '\0';
      hdr_end = strstr(buf, "\r\n\r\n");
    }
    if (!hdr_end)
      return -1;

    size_t hlen = (size_t)(hdr_end - buf) + 4;
    if (hlen >= sizeof(r->headers))
      return -1;
    memcpy(r->headers, buf, hlen);
    r->headers[hlen] = '\0';
    r->header_len = hlen;

    if (sscanf(buf, "HTTP/1.%*d %d", &r->status) != 1)
      return -1;

    /* Skip interim 1xx (except 101 Switching Protocols) */
    if (r->status >= 100 && r->status < 200 && r->status != 101) {
      /* Consume the body (empty for 1xx) and retry */
      long clen = find_content_length(r->headers);
      if (clen > 0 && (size_t)clen < sizeof(r->body)) {
        size_t have = total - hlen;
        if (have < (size_t)clen) {
          while (r->body_len < (size_t)clen) {
            ssize_t n =
                recv(fd, r->body + r->body_len, (size_t)clen - r->body_len, 0);
            if (n <= 0)
              break;
            r->body_len += (size_t)n;
          }
        }
      }
      /* Compact buffer: move remaining data after this 1xx response */
      size_t consumed = hlen;
      if (total > consumed) {
        memmove(buf, buf + consumed, total - consumed);
        total -= consumed;
      } else {
        total = 0;
      }
      buf[total] = '\0';
      continue;
    }

    long clen = find_content_length(r->headers);
    if (is_head) {
      r->body_len = 0;
      return 0;
    }
    if (clen < 0)
      clen = 0;
    if ((size_t)clen >= sizeof(r->body))
      return -1;

    size_t have = total - hlen;
    if (have > (size_t)clen)
      have = (size_t)clen;
    memcpy(r->body, buf + hlen, have);
    r->body_len = have;
    while (r->body_len < (size_t)clen) {
      ssize_t n =
          recv(fd, r->body + r->body_len, (size_t)clen - r->body_len, 0);
      if (n <= 0)
        break;
      r->body_len += (size_t)n;
    }
    r->body[r->body_len] = '\0';
    return 0;
  }
}

/* One request on a fresh connection. */
static int do_request(uint16_t port, const char *raw, resp_t *r, int is_head) {
  int fd = client_connect(port);
  if (fd < 0)
    return -1;
  if (send(fd, raw, strlen(raw), 0) < 0) {
    close(fd);
    return -1;
  }
  int rc = read_response(fd, r, is_head);
  close(fd);
  return rc;
}

/* ── Dynamic handler under test ────────────────────────────── */
static bool health_handler(const scl_http_request_t *req,
                           scl_http_response_t *resp, void *user) {
  (void)user;
  if (scl_strcmp(req->path, "/health") == 0) {
    static const char json[] = "{\"status\":\"ok\"}";
    resp->status = 200;
    resp->content_type = "application/json";
    resp->body = json;
    resp->body_len = sizeof(json) - 1;
    return true;
  }
  /* Echo body back (for body-framing tests) */
  if (scl_strcmp(req->path, "/echo") == 0) {
    resp->status = 200;
    resp->content_type =
        req->content_type ? req->content_type : "application/octet-stream";
    resp->body = req->body;
    resp->body_len = req->body_len;
    return true;
  }
  /* Return info about uploads parsed (for multipart tests) */
  if (scl_strcmp(req->path, "/upload") == 0) {
    static char infobuf[4096];
    size_t pos = 0;
    pos += snprintf(infobuf + pos, sizeof(infobuf) - pos, "{\"uploads\":%zu,",
                    req->upload_count);
    for (size_t i = 0; i < req->upload_count && pos < sizeof(infobuf); i++) {
      const scl_http_upload_t *u = &req->uploads[i];
      if (u->filename)
        pos += snprintf(infobuf + pos, sizeof(infobuf) - pos, "\"%s\":\"%s\",",
                        u->name ? u->name : "", u->filename);
      else
        pos += snprintf(infobuf + pos, sizeof(infobuf) - pos,
                        "\"name%zu\":\"%s\",", i, u->name ? u->name : "");
    }
    if (pos > 0 && pos < sizeof(infobuf))
      infobuf[pos - 1] = '}';
    resp->status = 200;
    resp->content_type = "application/json";
    resp->body = infobuf;
    resp->body_len = scl_strlen(infobuf);
    return true;
  }
  return false;
}

/* ── Fixture: temp docroot ─────────────────────────────────── */
static void write_file(const char *path, const char *content) {
  FILE *f = fopen(path, "wb");
  if (f) {
    fputs(content, f);
    fclose(f);
  }
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
  snprintf(p, sizeof(p), "%s/index.html", root);
  write_file(p, "<h1>home</h1>\n");
  snprintf(p, sizeof(p), "%s/hello.txt", root);
  write_file(p, "hello world\n");
  snprintf(p, sizeof(p), "%s/data.json", root);
  write_file(p, "{\"k\":1}\n");
  char subdir[300];
  snprintf(subdir, sizeof(subdir), "%s/sub", root);
  mkdir(subdir, 0755);
  snprintf(p, sizeof(p), "%s/sub/page.css", root);
  write_file(p, "body{}\n");

  scl_http_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.host = "127.0.0.1";
  cfg.port = 0; /* ephemeral */
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
  if (do_request(
          port,
          "GET /hello.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r,
          0) == 0) {
    SCL_EXPECT_EQ_I(&tr, r.status, 200);
    SCL_EXPECT_EQ_STR(&tr, r.body, "hello world\n");
    SCL_EXPECT_TRUE(&tr, header_has(&r, "text/plain"));
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: directory index");
  if (do_request(port, "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
                 &r, 0) == 0) {
    SCL_EXPECT_EQ_I(&tr, r.status, 200);
    SCL_EXPECT_TRUE(&tr, strstr(r.body, "home") != NULL);
    SCL_EXPECT_TRUE(&tr, header_has(&r, "text/html"));
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: MIME by extension");
  if (do_request(
          port,
          "GET /data.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r,
          0) == 0) {
    SCL_EXPECT_EQ_I(&tr, r.status, 200);
    SCL_EXPECT_TRUE(&tr, header_has(&r, "application/json"));
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }
  if (do_request(
          port,
          "GET /sub/page.css HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
          &r, 0) == 0) {
    SCL_EXPECT_EQ_I(&tr, r.status, 200);
    SCL_EXPECT_TRUE(&tr, header_has(&r, "text/css"));
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: 404 for missing file");
  if (do_request(
          port,
          "GET /nope.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", &r,
          0) == 0)
    SCL_EXPECT_EQ_I(&tr, r.status, 404);
  else
    SCL_EXPECT_TRUE(&tr, 0);

  scl_test_group("HTTP: path traversal is blocked");
  /* literal ../ */
  if (do_request(port,
                 "GET /../../../etc/passwd HTTP/1.1\r\nHost: x\r\nConnection: "
                 "close\r\n\r\n",
                 &r, 0) == 0) {
    SCL_EXPECT_TRUE(&tr, r.status == 403 || r.status == 404);
    SCL_EXPECT_TRUE(&tr, strstr(r.body, "root:") == NULL);
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }
  /* percent-encoded ../ */
  if (do_request(port,
                 "GET /%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: "
                 "x\r\nConnection: close\r\n\r\n",
                 &r, 0) == 0) {
    SCL_EXPECT_TRUE(&tr, r.status == 403 || r.status == 404);
    SCL_EXPECT_TRUE(&tr, strstr(r.body, "root:") == NULL);
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: encoded NUL rejected");
  if (do_request(port,
                 "GET /hello.txt%00.png HTTP/1.1\r\nHost: x\r\nConnection: "
                 "close\r\n\r\n",
                 &r, 0) == 0)
    SCL_EXPECT_EQ_I(&tr, r.status, 400);
  else
    SCL_EXPECT_TRUE(&tr, 0);

  scl_test_group("HTTP: method not allowed / version");
  if (do_request(port,
                 "POST /hello.txt HTTP/1.1\r\nHost: x\r\nContent-Length: "
                 "0\r\nConnection: close\r\n\r\n",
                 &r, 0) == 0)
    SCL_EXPECT_EQ_I(&tr, r.status, 405);
  else
    SCL_EXPECT_TRUE(&tr, 0);
  if (do_request(
          port,
          "GET /hello.txt HTTP/2.0\r\nHost: x\r\nConnection: close\r\n\r\n", &r,
          0) == 0)
    SCL_EXPECT_EQ_I(&tr, r.status, 505);
  else
    SCL_EXPECT_TRUE(&tr, 0);

  scl_test_group("HTTP: malformed request line -> 400");
  if (do_request(port, "GET\r\n\r\n", &r, 0) == 0)
    SCL_EXPECT_EQ_I(&tr, r.status, 400);
  else
    SCL_EXPECT_TRUE(&tr, 0);

  /* Oversized header block fills the connection buffer with no end-of-headers
   * and must yield 431. This also exercises send_error()'s longest reason
   * phrase ("Request Header Fields Too Large") — under ASan it proves the
   * error-body length is clamped to the buffer (no stack over-read). */
  scl_test_group("HTTP: oversized headers -> 431 (no over-read)");
  {
    size_t big = 128 * 1024;
    char *raw = (char *)malloc(big + 64);
    if (raw) {
      int hn = snprintf(raw, 64, "GET / HTTP/1.1\r\nX: ");
      memset(raw + hn, 'a', big); /* no terminating CRLFCRLF */
      raw[hn + big] = '\0';
      if (do_request(port, raw, &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 431);
      else
        SCL_EXPECT_TRUE(&tr, 0);
      free(raw);
    }
  }

  scl_test_group("HTTP: HEAD returns headers, no body");
  if (do_request(
          port,
          "HEAD /hello.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
          &r, 1) == 0) {
    SCL_EXPECT_EQ_I(&tr, r.status, 200);
    SCL_EXPECT_EQ_SZ(&tr, r.body_len, 0);
    SCL_EXPECT_TRUE(&tr, header_has(&r, "Content-Length:"));
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: dynamic handler");
  if (do_request(port,
                 "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
                 &r, 0) == 0) {
    SCL_EXPECT_EQ_I(&tr, r.status, 200);
    SCL_EXPECT_EQ_STR(&tr, r.body, "{\"status\":\"ok\"}");
    SCL_EXPECT_TRUE(&tr, header_has(&r, "application/json"));
  } else {
    SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: keep-alive serves multiple requests");
  {
    int fd = client_connect(port);
    SCL_EXPECT_TRUE(&tr, fd >= 0);
    if (fd >= 0) {
      const char *req1 = "GET /hello.txt HTTP/1.1\r\nHost: x\r\n\r\n";
      const char *req2 =
          "GET /data.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
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

  scl_test_group("HTTP: POST with Content-Length body");
  {
    const char *body = "hello=world&foo=bar";
    char req[1024];
    snprintf(req, sizeof(req),
             "POST /echo HTTP/1.1\r\n"
             "Host: x\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, body);
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: POST with chunked body");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Content-Type: text/plain\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "5\r\n"
                      "Hello\r\n"
                      "6\r\n"
                      " world\r\n"
                      "0\r\n"
                      "\r\n";
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, "Hello world");
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: chunked with chunk-extensions (ignored per spec)");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "5;foo=bar\r\n"
                      "Hello\r\n"
                      "0;trailer-ext\r\n"
                      "\r\n";
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, "Hello");
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: duplicate Content-Length, same value -> ok");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 5\r\n"
                      "Content-Length: 5\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "Hello";
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, "Hello");
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: duplicate Content-Length, differing values -> 400");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Content-Length: 5\r\n"
                      "Content-Length: 6\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "Hello";
    if (do_request(port, req, &r, 0) == 0)
      SCL_EXPECT_EQ_I(&tr, r.status, 400);
    else
      SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group(
      "HTTP: Transfer-Encoding takes precedence over Content-Length");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Content-Length: 100\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "5\r\n"
                      "Hello\r\n"
                      "0\r\n"
                      "\r\n";
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, "Hello");
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: chunked not last encoding -> 501");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Transfer-Encoding: chunked, gzip\r\n"
                      "Connection: close\r\n"
                      "\r\n";
    if (do_request(port, req, &r, 0) == 0)
      SCL_EXPECT_EQ_I(&tr, r.status, 501);
    else
      SCL_EXPECT_TRUE(&tr, 0);
  }

  scl_test_group("HTTP: multiple Transfer-Encoding headers concatenated");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Transfer-Encoding: gzip\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "5\r\n"
                      "Hello\r\n"
                      "0\r\n"
                      "\r\n";
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, "Hello");
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: Expect: 100-continue with body");
  {
    const char *body = "continue-body";
    char req[1024];
    snprintf(req, sizeof(req),
             "POST /echo HTTP/1.1\r\n"
             "Host: x\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n"
             "Expect: 100-continue\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_STR(&tr, r.body, body);
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: multipart/form-data basic upload");
  {
    const char *body = "--bound\r\n"
                       "Content-Disposition: form-data; name=\"file\"; "
                       "filename=\"test.txt\"\r\n"
                       "Content-Type: text/plain\r\n"
                       "\r\n"
                       "file content here\r\n"
                       "--bound\r\n"
                       "Content-Disposition: form-data; name=\"field\"\r\n"
                       "\r\n"
                       "value\r\n"
                       "--bound--\r\n";
    char req[2048];
    snprintf(req, sizeof(req),
             "POST /upload HTTP/1.1\r\n"
             "Host: x\r\n"
             "Content-Type: multipart/form-data; boundary=bound\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_TRUE(&tr, strstr(r.body, "\"uploads\":2") != NULL);
      SCL_EXPECT_TRUE(&tr, strstr(r.body, "\"file\":\"test.txt\"") != NULL);
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: body exceeds max_body_size -> 413");
  {
    /* Build a request body larger than the default 1MB limit */
    size_t big = 1024 * 1024 + 1;
    char *big_body = (char *)malloc(big);
    if (big_body) {
      memset(big_body, 'A', big);
      char req[8192];
      snprintf(req, sizeof(req),
               "POST /echo HTTP/1.1\r\n"
               "Host: x\r\n"
               "Content-Length: %zu\r\n"
               "Connection: close\r\n"
               "\r\n",
               big);
      if (do_request(port, req, &r, 0) == 0)
        SCL_EXPECT_EQ_I(&tr, r.status, 413);
      else
        SCL_EXPECT_TRUE(&tr, 0);
      free(big_body);
    }
  }

  scl_test_group("HTTP: headers-only POST (Content-Length: 0)");
  {
    const char *req = "POST /echo HTTP/1.1\r\n"
                      "Host: x\r\n"
                      "Content-Length: 0\r\n"
                      "Connection: close\r\n"
                      "\r\n";
    if (do_request(port, req, &r, 0) == 0) {
      SCL_EXPECT_EQ_I(&tr, r.status, 200);
      SCL_EXPECT_EQ_SZ(&tr, r.body_len, 0);
    } else {
      SCL_EXPECT_TRUE(&tr, 0);
    }
  }

  scl_test_group("HTTP: keep-alive with POST body");
  {
    int fd = client_connect(port);
    SCL_EXPECT_TRUE(&tr, fd >= 0);
    if (fd >= 0) {
      const char *req1 = "POST /echo HTTP/1.1\r\n"
                         "Host: x\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 5\r\n"
                         "\r\n"
                         "Hello";
      const char *req2 = "POST /echo HTTP/1.1\r\n"
                         "Host: x\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 5\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "World";
      send(fd, req1, strlen(req1), 0);
      int ok1 = read_response(fd, &r, 0);
      int s1 = r.status;
      size_t b1 = r.body_len;
      send(fd, req2, strlen(req2), 0);
      int ok2 = read_response(fd, &r, 0);
      int s2 = r.status;
      size_t b2 = r.body_len;
      SCL_EXPECT_EQ_I(&tr, ok1, 0);
      SCL_EXPECT_EQ_I(&tr, ok2, 0);
      SCL_EXPECT_EQ_I(&tr, s1, 200);
      SCL_EXPECT_EQ_I(&tr, s2, 200);
      SCL_EXPECT_EQ_SZ(&tr, b1, 5);
      SCL_EXPECT_EQ_SZ(&tr, b2, 5);
      close(fd);
    }
  }

  scl_test_group("HTTP: keep-alive with chunked POST and pipelining");
  {
    int fd = client_connect(port);
    SCL_EXPECT_TRUE(&tr, fd >= 0);
    if (fd >= 0) {
      const char *req1 = "POST /echo HTTP/1.1\r\n"
                         "Host: x\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n"
                         "5\r\n"
                         "Heelo\r\n"
                         "0\r\n"
                         "\r\n";
      const char *req2 = "POST /echo HTTP/1.1\r\n"
                         "Host: x\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "5\r\n"
                         "World\r\n"
                         "0\r\n"
                         "\r\n";
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
  snprintf(p, sizeof(p), "%s/index.html", root);
  unlink(p);
  snprintf(p, sizeof(p), "%s/hello.txt", root);
  unlink(p);
  snprintf(p, sizeof(p), "%s/data.json", root);
  unlink(p);
  snprintf(p, sizeof(p), "%s/sub/page.css", root);
  unlink(p);
  snprintf(p, sizeof(p), "%s/sub", root);
  rmdir(p);
  rmdir(root);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
