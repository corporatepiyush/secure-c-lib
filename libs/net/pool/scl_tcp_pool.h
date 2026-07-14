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

/* Lock-free TCP connection pool. Treiber stack (LIFO hot) + Vyukov MPMC
 * (balanced). Cache-line padded head/tail. DWCAS ABA-safe. */

#ifndef SCL_TCP_POOL_H
#define SCL_TCP_POOL_H

/*
 * scl_tcp_pool — a fixed-capacity TCP connection pool built entirely on the
 * lock-free primitives in scl_concurrent_common.h:
 *
 *   - free list of connection slots : scl_lfstack_t  (ABA-safe Treiber stack)
 *   - hand-off queue of accepted fds: scl_mpmc_queue_t (Vyukov bounded MPMC)
 *
 * Connection objects are pre-allocated once and recycled forever — never
 * free()d while the pool is live — which is exactly the invariant the
 * lock-free stack requires for memory safety. acquire()/release() and the
 * ready-queue post/get paths take no locks.
 *
 * ── Security design ──────────────────────────────────────────────────────────
 *
 * The pool provides the foundation for the HTTP server's connection handling;
 * its security posture is therefore critical:
 *
 *   1. Pre-allocation: All connection slots and read buffers are allocated
 *      once at init() time. This eliminates use-after-free on the data-path
 *      (the most common lock-free memory-safety bug) and bounds the server's
 *      memory footprint regardless of how many connections arrive.
 *
 *   2. Buffer scrubbing: Release() calls scl_secure_zero on the connection's
 *      read buffer so no request/response data survives between users. This
 *      prevents a malicious client from finding another user's data in the
 *      buffer (the pool analogue of Heartbleed's information disclosure).
 *
 *   3. Capacity limits: acquire() returns NULL when all slots are busy,
 *      bounding the number of simultaneously handled connections. The caller
 *      (the server's acceptor) drops excess connections rather than queuing
 *      them indefinitely, providing implicit back-pressure.
 *
 *   4. No inline allocation on the data-path: acquire, post, get, release all
 *      operate on pre-existing slot objects — no malloc/free. This eliminates
 *      entire classes of memory-exhaustion and fragmentation attacks.
 *
 * ── Why lock-free? ───────────────────────────────────────────────────────────
 *
 * A mutex-based pool would serialise the accept/dispatch paths under high
 * concurrency. Lock-free (stack + MPMC) means the acceptor thread never blocks
 * when handing a connection to a worker, and workers never block when returning
 * a slot. The only blocking primitive in the server is a condvar that parks
 * *idle* workers — not the critical path.
 *
 * ── Integration note ─────────────────────────────────────────────────────────
 *
 * The pool is designed to be embedded in a server (like scl_http_server) but
 * is generic enough for any TCP-based worker-pool pattern. The DDoS mitigation
 * module (scl_net_ddos.h) can be plugged in at the accept/post boundary.
 *
 * The socket helpers are POSIX-only (portable across Linux/macOS/BSD); no
 * OS-specific #ifdef appears here, per project policy.
 */

#include "scl_atomic.h"
#include "scl_common.h"
#include "scl_concurrent_common.h"

/*
 * The lock-free data structure headers provide the concrete types
 * (scl_lfstack_t, scl_mpmc_queue_t) that the pool uses for the free
 * list and the ready queue. These were formerly part of
 * scl_concurrent_common.h but were factored out into their own
 * headers under structures/ for better separation of concerns.
 */
#include "scl_concurrent_lfstack.h" /* scl_lfstack_t — Treiber stack */
#include "scl_concurrent_mpmc.h"    /* scl_mpmc_queue_t — Vyukov MPMC */

#include <netinet/in.h>
#include <sys/socket.h>

/* One reusable connection. The first member MUST be the lock-free stack
 * link (scl_lfstack stores its `next` pointer there). */
typedef struct scl_tcp_conn {
  uintptr_t _next;              /* lfstack link — keep first */
  int fd;                       /* socket fd, or -1 when idle */
  uint32_t index;               /* slot index (immutable) */
  struct sockaddr_storage peer; /* peer address */
  socklen_t peer_len;
  unsigned char *rbuf; /* per-connection read buffer */
  size_t rbuf_cap;
  size_t rbuf_len; /* valid bytes in rbuf */
  void *user;      /* opaque per-connection state */
} scl_tcp_conn_t;

typedef struct {
  scl_allocator_t *alloc;
  scl_tcp_conn_t *slots;     /* contiguous slot array */
  unsigned char *rbuf_arena; /* contiguous read buffers */
  size_t capacity;
  size_t rbuf_cap;

  scl_lfstack_t free_list; /* available slots (lock-free) */
  scl_mpmc_queue_t ready;  /* accepted, awaiting a worker */

  /* Observability (relaxed counters). */
  scl_atomic_size_t live; /* currently acquired */
  scl_atomic_size_t total_acquired;
  scl_atomic_size_t acquire_failures;
} scl_tcp_pool_t;

/* ── Pool lifecycle ─────────────────────────────────────────── */
scl_error_t scl_tcp_pool_init(scl_allocator_t *alloc, scl_tcp_pool_t *pool,
                              size_t capacity, size_t rbuf_cap);
void scl_tcp_pool_destroy(scl_tcp_pool_t *pool);

/* Acquire an idle slot (lock-free pop). Returns NULL if the pool is at
 * capacity; the failure is counted in acquire_failures. */
scl_tcp_conn_t *scl_tcp_pool_acquire(scl_tcp_pool_t *pool);

/* Return a slot to the pool. Closes conn->fd if still open and scrubs the
 * read buffer (secure-zeroed) so no payload lingers in recycled memory. */
void scl_tcp_pool_release(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn);

/* Hand an acquired connection to a worker via the lock-free ready queue.
 * Returns false if the queue is momentarily full (caller decides policy). */
bool scl_tcp_pool_post_ready(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn);
scl_tcp_conn_t *scl_tcp_pool_get_ready(scl_tcp_pool_t *pool);

size_t scl_tcp_pool_live(const scl_tcp_pool_t *pool);

/* ── Portable socket helpers (POSIX) ────────────────────────── */

/* Idempotently ignore SIGPIPE so a write to a half-closed peer returns
 * EPIPE instead of killing the process. Call once at startup. */
void scl_net_init(void);

/* Create a listening TCP socket bound to host:port (host NULL => any).
 * Sets SO_REUSEADDR. Returns the fd in *out_fd. */
scl_error_t scl_tcp_listen(const char *host, uint16_t port, int backlog,
                           int *out_fd);

/* Accept one connection into `conn` (sets fd, peer, TCP_NODELAY).
 * Returns SCL_OK, SCL_ERR_TIMEOUT (would-block), or SCL_ERR_IO. */
scl_error_t scl_tcp_accept(int listen_fd, scl_tcp_conn_t *conn);

scl_error_t scl_tcp_set_nonblocking(int fd, bool on);
scl_error_t scl_tcp_set_recv_timeout(int fd, int64_t ms);

/* Bound how long a blocking send() may stall before returning (SO_SNDTIMEO).
 * Without this a single slow-reading peer can pin a worker thread forever in
 * scl_tcp_send_all — a trivial slow-read DoS. ms <= 0 disables the timeout. */
scl_error_t scl_tcp_set_send_timeout(int fd, int64_t ms);

/* send() the whole buffer, retrying short writes. Returns SCL_OK or SCL_ERR_IO.
 */
scl_error_t scl_tcp_send_all(int fd, const void *buf, size_t len);

#endif /* SCL_TCP_POOL_H */
