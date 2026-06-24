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
 * The socket helpers are POSIX-only (portable across Linux/macOS/BSD); no
 * OS-specific #ifdef appears here, per project policy.
 */

#include "scl_common.h"
#include "scl_atomic.h"
#include "scl_concurrent_common.h"

/*
 * The lock-free data structure headers provide the concrete types
 * (scl_lfstack_t, scl_mpmc_queue_t) that the pool uses for the free
 * list and the ready queue. These were formerly part of
 * scl_concurrent_common.h but were factored out into their own
 * headers under structures/ for better separation of concerns.
 */
#include "scl_concurrent_lfstack.h"   /* scl_lfstack_t — Treiber stack */
#include "scl_concurrent_mpmc.h"      /* scl_mpmc_queue_t — Vyukov MPMC */

#include <sys/socket.h>
#include <netinet/in.h>

/* One reusable connection. The first member MUST be the lock-free stack
 * link (scl_lfstack stores its `next` pointer there). */
typedef struct scl_tcp_conn {
    uintptr_t               _next;      /* lfstack link — keep first */
    int                     fd;         /* socket fd, or -1 when idle */
    uint32_t                index;      /* slot index (immutable) */
    struct sockaddr_storage peer;       /* peer address */
    socklen_t               peer_len;
    unsigned char          *rbuf;       /* per-connection read buffer */
    size_t                  rbuf_cap;
    size_t                  rbuf_len;   /* valid bytes in rbuf */
    void                   *user;       /* opaque per-connection state */
} scl_tcp_conn_t;

typedef struct {
    scl_allocator_t  *alloc;
    scl_tcp_conn_t   *slots;            /* contiguous slot array */
    unsigned char    *rbuf_arena;       /* contiguous read buffers */
    size_t            capacity;
    size_t            rbuf_cap;

    scl_lfstack_t     free_list;        /* available slots (lock-free) */
    scl_mpmc_queue_t  ready;            /* accepted, awaiting a worker */

    /* Observability (relaxed counters). */
    scl_atomic_size_t live;            /* currently acquired */
    scl_atomic_size_t total_acquired;
    scl_atomic_size_t acquire_failures;
} scl_tcp_pool_t;

/* ── Pool lifecycle ─────────────────────────────────────────── */
scl_error_t scl_tcp_pool_init(scl_allocator_t *alloc, scl_tcp_pool_t *pool,
                              size_t capacity, size_t rbuf_cap);
void        scl_tcp_pool_destroy(scl_tcp_pool_t *pool);

/* Acquire an idle slot (lock-free pop). Returns NULL if the pool is at
 * capacity; the failure is counted in acquire_failures. */
scl_tcp_conn_t *scl_tcp_pool_acquire(scl_tcp_pool_t *pool);

/* Return a slot to the pool. Closes conn->fd if still open and scrubs the
 * read buffer (secure-zeroed) so no payload lingers in recycled memory. */
void scl_tcp_pool_release(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn);

/* Hand an acquired connection to a worker via the lock-free ready queue.
 * Returns false if the queue is momentarily full (caller decides policy). */
bool            scl_tcp_pool_post_ready(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn);
scl_tcp_conn_t *scl_tcp_pool_get_ready(scl_tcp_pool_t *pool);

size_t scl_tcp_pool_live(const scl_tcp_pool_t *pool);

/* ── Portable socket helpers (POSIX) ────────────────────────── */

/* Idempotently ignore SIGPIPE so a write to a half-closed peer returns
 * EPIPE instead of killing the process. Call once at startup. */
void scl_net_init(void);

/* Create a listening TCP socket bound to host:port (host NULL => any).
 * Sets SO_REUSEADDR. Returns the fd in *out_fd. */
scl_error_t scl_tcp_listen(const char *host, uint16_t port, int backlog, int *out_fd);

/* Accept one connection into `conn` (sets fd, peer, TCP_NODELAY).
 * Returns SCL_OK, SCL_ERR_TIMEOUT (would-block), or SCL_ERR_IO. */
scl_error_t scl_tcp_accept(int listen_fd, scl_tcp_conn_t *conn);

scl_error_t scl_tcp_set_nonblocking(int fd, bool on);
scl_error_t scl_tcp_set_recv_timeout(int fd, int64_t ms);

/* send() the whole buffer, retrying short writes. Returns SCL_OK or SCL_ERR_IO. */
scl_error_t scl_tcp_send_all(int fd, const void *buf, size_t len);

#endif /* SCL_TCP_POOL_H */
