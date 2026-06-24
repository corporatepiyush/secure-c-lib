/* Concurrency tests for scl_tcp_pool's lock-free paths (acquire/release and
 * the ready-queue handoff). Designed to fault under TSan and to detect any
 * slot being handed to two threads at once. No sockets are involved. */
#include "scl_test.h"
#include "scl_tcp_pool.h"
#include <stdatomic.h>

#define POOL_CAP   48
#define ITERS      30000

typedef struct {
    scl_tcp_pool_t pool;
    atomic_int     held[POOL_CAP];     /* per-slot ownership guard */
    atomic_int     produced;
    atomic_int     consumed;
} ctx_t;

/* ── Free-list churn: acquire -> own exclusively -> release ──── */
static void *freelist_worker(void *p) {
    scl_test_thread_arg_t *arg = (scl_test_thread_arg_t *)p;
    ctx_t *ctx = (ctx_t *)arg->user_data;
    scl_test_barrier_wait(arg->barrier);

    for (int i = 0; i < ITERS; i++) {
        scl_tcp_conn_t *c = scl_tcp_pool_acquire(&ctx->pool);
        if (!c) continue;                          /* exhausted: retry */
        int prev = atomic_exchange_explicit(&ctx->held[c->index], 1, memory_order_acq_rel);
        SCL_CC_EXPECT(arg->cc, prev == 0);          /* no double ownership */
        atomic_store_explicit(&ctx->held[c->index], 0, memory_order_release);
        scl_tcp_pool_release(&ctx->pool, c);
    }
    return NULL;
}

static void test_pool_freelist(scl_test_runner_t *tr) {
    scl_test_group("TCP pool: concurrent acquire/release conserves slots");
    ctx_t *ctx = (ctx_t *)calloc(1, sizeof(ctx_t));
    SCL_EXPECT_NOT_NULL(tr, ctx);
    if (!ctx) return;

    SCL_EXPECT_OK(tr, scl_tcp_pool_init(scl_allocator_default(), &ctx->pool, POOL_CAP, 256));
    for (int i = 0; i < POOL_CAP; i++) atomic_init(&ctx->held[i], 0);

    scl_test_run_concurrent(tr, 8, freelist_worker, ctx);

    /* All slots must be free again: capacity acquires must all succeed. */
    SCL_EXPECT_EQ_SZ(tr, scl_tcp_pool_live(&ctx->pool), 0);
    int got = 0;
    scl_tcp_conn_t *c;
    while ((c = scl_tcp_pool_acquire(&ctx->pool)) != NULL) got++;
    SCL_EXPECT_EQ_I(tr, got, POOL_CAP);

    scl_tcp_pool_destroy(&ctx->pool);
    free(ctx);
}

/* ── Ready-queue handoff: producers post, consumers drain ───── */
static void *handoff_worker(void *p) {
    scl_test_thread_arg_t *arg = (scl_test_thread_arg_t *)p;
    ctx_t *ctx = (ctx_t *)arg->user_data;
    int half = arg->nthreads / 2;
    int target = (arg->nthreads / 2) * ITERS;
    scl_test_barrier_wait(arg->barrier);

    if (arg->thread_id < half) {                    /* producer */
        for (int i = 0; i < ITERS; i++) {
            scl_tcp_conn_t *c;
            while ((c = scl_tcp_pool_acquire(&ctx->pool)) == NULL)
                scl_cpu_pause();                     /* wait for a free slot */
            while (!scl_tcp_pool_post_ready(&ctx->pool, c))
                scl_cpu_pause();                     /* ready queue full: spin */
            atomic_fetch_add_explicit(&ctx->produced, 1, memory_order_release);
        }
    } else {                                         /* consumer */
        for (;;) {
            scl_tcp_conn_t *c = scl_tcp_pool_get_ready(&ctx->pool);
            if (c) {
                int prev = atomic_exchange_explicit(&ctx->held[c->index], 1, memory_order_acq_rel);
                SCL_CC_EXPECT(arg->cc, prev == 0);   /* delivered to one consumer */
                atomic_store_explicit(&ctx->held[c->index], 0, memory_order_release);
                atomic_fetch_add_explicit(&ctx->consumed, 1, memory_order_relaxed);
                scl_tcp_pool_release(&ctx->pool, c);
            } else {
                if (atomic_load_explicit(&ctx->produced, memory_order_acquire) == target &&
                    atomic_load_explicit(&ctx->consumed, memory_order_acquire) == target)
                    break;
                scl_cpu_pause();
            }
        }
    }
    return NULL;
}

static void test_pool_handoff(scl_test_runner_t *tr) {
    scl_test_group("TCP pool: ready-queue handoff delivers each slot once");
    ctx_t *ctx = (ctx_t *)calloc(1, sizeof(ctx_t));
    SCL_EXPECT_NOT_NULL(tr, ctx);
    if (!ctx) return;

    SCL_EXPECT_OK(tr, scl_tcp_pool_init(scl_allocator_default(), &ctx->pool, POOL_CAP, 256));
    for (int i = 0; i < POOL_CAP; i++) atomic_init(&ctx->held[i], 0);
    atomic_init(&ctx->produced, 0);
    atomic_init(&ctx->consumed, 0);

    scl_test_run_concurrent(tr, 8, handoff_worker, ctx);   /* 4 producers, 4 consumers */

    SCL_EXPECT_EQ_I(tr, atomic_load(&ctx->consumed), (8 / 2) * ITERS);
    SCL_EXPECT_EQ_SZ(tr, scl_tcp_pool_live(&ctx->pool), 0);

    scl_tcp_pool_destroy(&ctx->pool);
    free(ctx);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_pool_freelist(&tr);
    test_pool_handoff(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
