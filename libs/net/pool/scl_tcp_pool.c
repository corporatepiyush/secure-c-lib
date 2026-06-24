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

/* ── Pool lifecycle ─────────────────────────────────────────── */

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

bool scl_tcp_pool_post_ready(scl_tcp_pool_t *pool, scl_tcp_conn_t *conn) {
    if (scl_unlikely(!pool || !conn)) return false;
    return scl_mpmc_enqueue(&pool->ready, (uintptr_t)conn);
}

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

/* ── Portable socket helpers ────────────────────────────────── */

static scl_once_t scl_net_once = SCL_ONCE_INIT;
static void scl_net_once_fn(void) { signal(SIGPIPE, SIG_IGN); }

void scl_net_init(void) {
    scl_once(&scl_net_once, scl_net_once_fn);
}

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

scl_error_t scl_tcp_send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) { sent += (size_t)n; continue; }
        if (n < 0 && (errno == EINTR)) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        return SCL_ERR_IO;
    }
    return SCL_OK;
}
