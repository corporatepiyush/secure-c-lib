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

/* Hardened socket wrappers. EINTR retry on recv/send/accept, SIGPIPE suppression (MSG_NOSIGNAL/SO_NOSIGPIPE), forced TCP_NODELAY, non-blocking connect helper. */

#ifndef SCL_SOCKET_H
#define SCL_SOCKET_H

/*
 * scl_socket — proxy adapter over the native BSD-sockets API.
 *
 * Purpose (the "secure C lib" thesis): callers never touch raw sockets.
 * Every native call is funneled through a hardened wrapper that bakes in the
 * defaults that popular C networking code learns the hard way:
 *   - EINTR is retried, never surfaced;
 *   - SIGPIPE is disarmed process-wide (scl_net_init) so a write to a dead
 *     peer returns an error instead of killing the process;
 *   - accepted sockets are forced blocking + TCP_NODELAY (BSD/macOS otherwise
 *     inherit the listener's O_NONBLOCK — a classic portability trap);
 *   - errno is mapped to scl_error_t; recv outcomes are an explicit enum;
 *   - peer addresses are surfaced as a flat scl_peer_t (no sockaddr leakage).
 *
 * All OS-specific details live in scl_socket.c (an allowed location for
 * platform #ifdef). Callers include only this header.
 */

#include "scl_common.h"
#include <stdint.h>
#include <stdbool.h>

/* Flattened peer address — no sockaddr in caller code. */
typedef struct {
    char     ip[46];   /* INET6_ADDRSTRLEN */
    uint16_t port;
} scl_peer_t;

typedef enum {
    SCL_RECV_OK = 0,   /* *out_n bytes read */
    SCL_RECV_CLOSED,   /* peer performed orderly shutdown */
    SCL_RECV_TIMEOUT,  /* would block / SO_RCVTIMEO expired */
    SCL_RECV_ERROR
} scl_recv_status_t;

/* Idempotently disarm SIGPIPE. Safe to call many times / from many threads. */
void scl_net_init(void);

/* Create a listening TCP socket bound to host:port (host NULL => any).
 * Sets SO_REUSEADDR; returns the fd in *out_fd. */
scl_error_t scl_socket_listen(const char *host, uint16_t port, int backlog, int *out_fd);

/* Accept one connection. Fills *out_fd and (if non-NULL) *peer. The accepted
 * socket is blocking with TCP_NODELAY set. SCL_ERR_TIMEOUT => would-block. */
scl_error_t scl_socket_accept(int listen_fd, int *out_fd, scl_peer_t *peer);

scl_error_t scl_socket_set_nonblocking(int fd, bool on);
scl_error_t scl_socket_set_nodelay(int fd, bool on);
scl_error_t scl_socket_set_recv_timeout(int fd, int64_t ms);

/* EINTR-safe receive. Returns a status; bytes read land in *out_n. */
scl_recv_status_t scl_socket_recv(int fd, void *buf, size_t len, size_t *out_n);

/* Send the whole buffer, retrying short/interrupted writes. */
scl_error_t scl_socket_send_all(int fd, const void *buf, size_t len);

void scl_socket_close(int fd);

/* Bound local port (resolves an ephemeral port chosen by the kernel). */
scl_error_t scl_socket_local_port(int fd, uint16_t *out_port);

/* Wait for readability. Returns 1 ready, 0 timeout (or EINTR), -1 error. */
int scl_socket_wait_readable(int fd, int timeout_ms);

#endif /* SCL_SOCKET_H */
