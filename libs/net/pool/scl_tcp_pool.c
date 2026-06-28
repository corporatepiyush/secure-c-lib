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

/* Lock-free TCP connection pool. Treiber stack (LIFO hot) + Vyukov MPMC (balanced). Cache-line padded head/tail. DWCAS ABA-safe. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O2")
#endif

#include "scl_tcp_pool.h"
#include "scl_string.h"
#include "scl_pthread.h"

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

/*
 * scl_tcp_pool.c — lock-free TCP connection pool implementation.
 *
 * ── What this file does ─────────────────────────────────────────────────────
 *
 * This is the realisation of the lock-free connection pool declared in
 * scl_tcp_pool.h. It manages a fixed set of pre-allocated "slot" objects:
 * each slot has an fd, a per-connection read buffer, and a lock-free stack
 * link member (used by the Treiber stack for the free list).
 *
 * The pool serves two masters simultaneously:
 *
 *   1. An acceptor thread (top-half) that calls acquire() to get a free slot,
 *      accept()s a TCP connection into it, then post_ready() to hand it off.
 *   2. Worker threads (bottom-half) that call get_ready() to receive the
 *      connection, service it, then release() to return the slot.
 *
 * Both halves are O(1) and lock-free — no mutex, no syscall on the hot path.
 *
 * ── Security design ─────────────────────────────────────────────────────────
 *
 *   • Pre-allocated: slots are allocated once at init() and never freed until
 *     destroy(). This prevents use-after-free bugs (the most common lock-free
 *     memory-safety issue) and bounds the server's memory footprint.
 *   • Buffer scrubbing: release() calls scl_secure_zero on the read buffer
 *     to prevent data leaking between connections (Heartbleed-class mitigation).
 *   • Overflow-safe allocation: the contiguous rbuf_arena uses scl_mul_overflow
 *     to guard against size_t wrap-around in capacity * rbuf_cap.
 *   • Capacity limit: acquire() returns NULL when the free list is empty.
 *     The caller must drop the connection, providing back-pressure.
 *
 * ── Lock-free correctness ───────────────────────────────────────────────────
 *
 * The Treiber stack (free_list) requires an ABA-prevention mechanism:
 * scl_lfstack uses tagged-pointers via scl_dwcas (16-byte CAS on x86-64,
 * double-word CAS via a retry loop elsewhere). The Vyukov MPMC queue (ready)
 * is a bounded FIFO with its own hazard-pointer-free design.
 *
 * Because slots are never freed, a slot pointer popped off the free list
 * is guaranteed to point to live memory — the only invariant the Treiber
 * stack requires for memory safety.
 *
 * ── Socket helpers ──────────────────────────────────────────────────────────
 *
 * The second half of this file provides portable POSIX socket wrappers:
 * listen, accept, send_all (retry on short write), non-blocking toggling,
 * and receive-timeout configuration. These are used both by the pool's
 * callers (scl_http_server) and by the HTTP client (scl_http_client).
 */

/* ── Pool lifecycle ─────────────────────────────────────────── */

/*
 * Initialise a connection pool with `capacity` slots, each having a read
 * buffer of `rbuf_cap` bytes.
 *
 * ── Allocation strategy ──────────────────────────────────────────────────────
 *
 * We allocate two contiguous arrays:
 *
 *   slots[0..capacity-1]  — each is a scl_tcp_conn_t (starts with the lfstack
 *                            link member for the Treiber stack's intrusive list)
 *   rbuf_arena            — one big block of capacity * rbuf_cap bytes, sliced
 *                            per-slot during init (no per-slot malloc overhead)
 *
 * The MPMC ready-queue is also heap-allocated (via scl_mpmc_init). The free
 * list is built by pushing every slot onto the stack — the acceptor pops them
 * and workers push them back.
 *
 * ── Error handling ──────────────────────────────────────────────────────────
 *
 * If any allocation fails, previously allocated resources are freed before
 * returning (partial-init cleanup). This is critical: a half-initialised pool
 * would corrupt on use.
 */
scl_error_t scl_tcp_pool_init(scl_allocator_t *alloc, scl_tcp_pool_t *pool,
                              size_t capacity, size_t rbuf_cap) {
    if (scl_unlikely(!alloc || !pool)) return SCL_ERR_NULL_PTR;
    if (capacity == 0 || rbuf_cap == 0) return SCL_ERR_INVALID_ARG;

    size_t arena_bytes;
    if (scl_mul_overflow(capacity, rbuf_cap, &arena_bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    (void)scl_memset(pool, 0, sizeof(*pool));
    pool->alloc    = alloc;
    pool->capacity = capacity;
    pool->rbuf_cap = rbuf_cap;

    pool->slots = (scl_tcp_conn_t *)scl_calloc(alloc, capacity,
                        sizeof(scl_tcp_conn_t), _Alignof(scl_tcp_conn_t));
    if (!pool->slots) return SCL_ERR_OUT_OF_MEMORY;

    pool->rbuf_arena = (unsigned char *)scl_alloc(alloc, arena_bytes,
                                                  _Alignof(max_align_t));
    if (!pool->rbuf_arena) {
        scl_free(alloc, pool->slots);
        pool->slots = NULL;
        return SCL_ERR_OUT_OF_MEMORY;
    }

    if (scl_mpmc_init(alloc, &pool->ready, capacity) != SCL_OK) {
        scl_free(alloc, pool->rbuf_arena);
        scl_free(alloc, pool->slots);
        pool->slots = NULL; pool->rbuf_arena = NULL;
        return SCL_ERR_OUT_OF_MEMORY;
    }

    scl_lfstack_init(&pool->free_list);
    atomic_init(&pool->live, 0);
    atomic_init(&pool->total_acquired, 0);
    atomic_init(&pool->acquire_failures, 0);

    /* Wire up slots and publish them onto the free list. */
    for (size_t i = 0; i < capacity; i++) {
        scl_tcp_conn_t *c = &pool->slots[i];
        c->fd       = -1;
        c->index    = (uint32_t)i;
        c->rbuf     = pool->rbuf_arena + i * rbuf_cap;
        c->rbuf_cap = rbuf_cap;
        c->rbuf_len = 0;
        scl_lfstack_push(&pool->free_list, c);
    }
    return SCL_OK;
}

/*
 * Destroy a pool: close every open fd, free the ready-queue, rbuf_arena, and
 * slots array. Idempotent when called on a zeroed-out pool.
 *
 * ── Thread safety ────────────────────────────────────────────────────────────
 *
 * The caller must ensure NO threads are accessing the pool when destroy() is
 * called. In the server lifecycle pattern, stop() joins all worker threads
 * before destroy().
 */
void scl_tcp_pool_destroy(scl_tcp_pool_t *pool) {
    if (!pool || !pool->slots) return;
    for (size_t i = 0; i < pool->capacity; i++) {
        if (pool->slots[i].fd >= 0) {
            close(pool->slots[i].fd);
            pool->slots[i].fd = -1;
        }
    }
    scl_mpmc_destroy(&pool->ready);
    scl_free(pool->alloc, pool->rbuf_arena);
    scl_free(pool->alloc, pool->slots);
    pool->slots = NULL;
    pool->rbuf_arena = NULL;
    pool->capacity = 0;
}

/*
 * Acquire a free connection slot from the lock-free stack.
 *
 * Returns NULL if the pool is at capacity (free list empty). The caller
 * should drop the connection rather than waiting — this provides natural
 * back-pressure against connection floods.
 *
 * The pop from scl_lfstack is O(1) and lock-free: it uses a 16-byte CAS
 * (tagged pointer) to prevent ABA. If the CAS fails due to contention, the
 * stack retries in a loop (bounded by ABA tag bits, essentially infinite).
 *
 * Security note: we clear fd, rbuf_len, peer_len, and user here to ensure
 * no stale data from the previous occupant leaks through. The rbuf itself
 * is scrubbed by release(), not here, because the slot is freshly popped.
 */
scl_tcp_conn_t *scl_tcp_pool_acquire(scl_tcp_pool_t *pool) {
    if (scl_unlikely(!pool)) return NULL;
    scl_tcp_conn_t *c = (scl_tcp_conn_t *)scl_lfstack_pop(&pool->free_list);
    if (!c) {
        atomic_fetch_add_explicit(&pool->acquire_failures, 1, memory_order_relaxed);
        return NULL;
    }
    c->fd       = -1;
    c->rbuf_len = 0;
    c->peer_len = 0;
    c->user     = NULL;
    atomic_fetch_add_explicit(&pool->live, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&pool->total_acquired, 1, memory_order_relaxed);
    return c;
}

/*
 * Return a slot to the pool's free list.
 *
 * ── What happens ─────────────────────────────────────────────────────────────
 *
 *   1. Close the socket fd (if still open). This signals the peer that we
 *      are done; the kernel tears down the TCP state.
 *   2. Scrub the read buffer with scl_secure_zero. This is the Heartbleed
 *      mitigation: no leftover request/response data survives to the next
 *      user of this slot.
 *   3. Reset metadata (rbuf_len, peer_len, user).
 *   4. Decrement the live counter.
 *   5. Push the slot onto the Treiber free list (lock-free).
 *
 * ⚠ IMPORTANT: After calling release(), the caller must NOT touch `conn` —
 * it may already have been popped by another thread's acquire().
 */
void scl_tcp_pool_release(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn) {
    if (scl_unlikely(!pool || !conn)) return;
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    /* Scrub any buffered request bytes before the slot is reused. */
    if (conn->rbuf && conn->rbuf_len)
        scl_secure_zero(conn->rbuf, conn->rbuf_len);
    conn->rbuf_len = 0;
    conn->peer_len = 0;
    conn->user     = NULL;
    atomic_fetch_sub_explicit(&pool->live, 1, memory_order_relaxed);
    scl_lfstack_push(&pool->free_list, conn);
}

/*
 * Post a connection to the ready queue for a worker to pick up.
 *
 * This is called by the acceptor after accept()ing a connection into an
 * acquired slot. The MPMC enqueue is lock-free and non-blocking; if the
 * queue is full (all workers busy), it returns false and the caller
 * (acceptor) should release the slot back to the free list.
 *
 * ── Back-pressure ────────────────────────────────────────────────────────────
 *
 * The ready queue has the same capacity as the pool itself. If enqueue fails,
 * it means EVERY slot is either:
 *   - being filled by the acceptor (acquired but not yet posted), or
 *   - being serviced by a worker (currently in handle_connection).
 * Dropping the connection is the correct behaviour — the kernel's listen
 * backlog provides a secondary queue while we drain.
 */
bool scl_tcp_pool_post_ready(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn) {
    if (scl_unlikely(!pool || !conn)) return false;
    return scl_mpmc_enqueue(&pool->ready, (uintptr_t)conn);
}

/*
 * Dequeue a connection from the ready queue (lock-free, non-blocking).
 *
 * Returns NULL if the queue is empty (no connections waiting). Workers call
 * this in a loop while the server is running; when the queue is empty they
 * park on the condvar semaphore (see scl_http_server.c:sem_wait_or_stop).
 *
 * The returned connection is ready for I/O: fd is set, peer is populated,
 * and the read buffer is empty (rbuf_len == 0).
 */
scl_tcp_conn_t *scl_tcp_pool_get_ready(scl_tcp_pool_t *pool) {
    if (scl_unlikely(!pool)) return NULL;
    uintptr_t v;
    if (!scl_mpmc_dequeue(&pool->ready, &v)) return NULL;
    return (scl_tcp_conn_t *)v;
}

size_t scl_tcp_pool_live(const scl_tcp_pool_t *pool) {
    if (!pool) return 0;
    return atomic_load_explicit(&pool->live, memory_order_relaxed);
}

/*
 * ── Portable socket helpers ─────────────────────────────────────────
 *
 * These wrappers provide a consistent error interface (scl_error_t) over
 * POSIX socket APIs. They are used by both the server (acceptor + workers)
 * and the HTTP client.
 *
 * Key design decisions:
 *
 *   • SIGPIPE is ignored (scl_net_init) so write() on a closed peer returns
 *     EPIPE instead of killing the process.
 *   • SO_REUSEADDR is always set on listen sockets to avoid "address in use"
 *     after a restart.
 *   • TCP_NODELAY is set on accepted sockets so responses are sent
 *     immediately (no Nagle buffering) — critical for HTTP response latency.
 *   • SO_RCVTIMEO provides per-socket receive deadlines without forcing
 *     non-blocking IO and an epoll/kqueue event loop.
 *   • scl_tcp_send_all retries short writes and EINTR/EAGAIN, guaranteeing
 *     the full buffer is sent before returning.
 *
 * Security note: All helpers use scl_unlikely() on error paths so the
 * compiler optimises for the success case (branch prediction hint).
 */

static scl_once_t scl_net_once = SCL_ONCE_INIT;
static void scl_net_once_fn(void) { signal(SIGPIPE, SIG_IGN); }

void scl_net_init(void) {
    scl_once(&scl_net_once, scl_net_once_fn);
}

/*
 * Create a listening TCP socket.
 *
 * Uses getaddrinfo to resolve the host (IPv4/IPv6 dual-stack when host is
 * NULL / "::" / "0.0.0.0"). Iterates over each address until one succeeds.
 * Sets SO_REUSEADDR so we can rebind immediately after a restart.
 *
 * ── Parameters ───────────────────────────────────────────────────────────────
 *   host    — bind address (NULL => all interfaces, "127.0.0.1" => loopback)
 *   port    — TCP port (0 => OS picks ephemeral, query via getsockname)
 *   backlog — listen(2) backlog (<=0 => SOMAXCONN)
 *   out_fd  — receives the socket fd on success
 */
scl_error_t scl_tcp_listen(const char *host, uint16_t port, int backlog, int *out_fd) {
    if (scl_unlikely(!out_fd)) return SCL_ERR_NULL_PTR;
    *out_fd = -1;

    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

    struct addrinfo hints;
    (void)scl_memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res)
        return SCL_ERR_IO;

    int fd = -1;
    scl_error_t err = SCL_ERR_IO;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 &&
            listen(fd, backlog > 0 ? backlog : SOMAXCONN) == 0) {
            err = SCL_OK;
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (err != SCL_OK) return err;

    *out_fd = fd;
    return SCL_OK;
}

/*
 * Accept a single incoming connection.
 *
 * On success, conn->fd is set, conn->peer holds the remote address, and
 * TCP_NODELAY is enabled. On macOS/BSD the accepted socket inherits the
 * listener's O_NONBLOCK, so we explicitly force blocking mode — worker
 * recv() relies on SO_RCVTIMEO, not on busy-polling EAGAIN.
 *
 * ── Error semantics ──────────────────────────────────────────────────────────
 *   SCL_OK           — connection accepted, conn is populated
 *   SCL_ERR_TIMEOUT  — would-block (EAGAIN/EWOULDBLOCK), caller should retry
 *   SCL_ERR_IO       — hard error
 */
scl_error_t scl_tcp_accept(int listen_fd, scl_tcp_conn_t *conn) {
    if (scl_unlikely(!conn)) return SCL_ERR_NULL_PTR;
    conn->peer_len = sizeof(conn->peer);
    int fd;
    do {
        fd = accept(listen_fd, (struct sockaddr *)&conn->peer, &conn->peer_len);
    } while (fd < 0 && errno == EINTR);

    if (fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return SCL_ERR_TIMEOUT;
        return SCL_ERR_IO;
    }

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    /* On BSD/macOS an accepted socket inherits the listener's O_NONBLOCK;
     * force blocking so worker recv() relies on SO_RCVTIMEO, not busy EAGAIN. */
    scl_tcp_set_nonblocking(fd, false);
    conn->fd = fd;
    return SCL_OK;
}

scl_error_t scl_tcp_set_nonblocking(int fd, bool on) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return SCL_ERR_IO;
    flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(fd, F_SETFL, flags) < 0) return SCL_ERR_IO;
    return SCL_OK;
}

scl_error_t scl_tcp_set_recv_timeout(int fd, int64_t ms) {
    struct timeval tv;
    tv.tv_sec  = (long)(ms / 1000);
    tv.tv_usec = (long)((ms % 1000) * 1000);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        return SCL_ERR_IO;
    return SCL_OK;
}

scl_error_t scl_tcp_set_send_timeout(int fd, int64_t ms) {
    struct timeval tv;
    tv.tv_sec  = (long)(ms / 1000);
    tv.tv_usec = (long)((ms % 1000) * 1000);
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        return SCL_ERR_IO;
    return SCL_OK;
}

/*
 * Write the entire buffer to a socket, retrying short writes.
 *
 * ── Why this exists ──────────────────────────────────────────────────────────
 *
 * A raw send(2) or write(2) may return fewer bytes than requested (partial
 * write due to kernel buffer space). This function loops until all bytes are
 * sent or a hard error occurs. Both EINTR and EAGAIN/EWOULDBLOCK are retried
 * (EAGAIN can happen on blocking sockets with a short kernel buffer).
 *
 * This is used by:
 *   • scl_http_server.c:send_simple / serve_static (sending responses)
 *   • scl_http_client.c:send_request (sending request headers + body)
 */
scl_error_t scl_tcp_send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) { sent += (size_t)n; continue; }
        if (n < 0 && (errno == EINTR)) continue;
        /* With SO_SNDTIMEO set, EAGAIN/EWOULDBLOCK means the peer stalled past
         * the send deadline. Do NOT spin forever (that pins the worker on a
         * slow-read DoS) — surface a timeout so the caller closes the conn. */
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return SCL_ERR_TIMEOUT;
        return SCL_ERR_IO;
    }
    return SCL_OK;
}
