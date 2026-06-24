/* Tests for the lock-free primitives baked into the core proxies:
 *   - scl_dwcas        (128-bit CAS, cmpxchg16b on x86-64)
 *   - scl_lfstack      (ABA-safe intrusive Treiber stack, pool free-list)
 *   - scl_mpmc_queue   (Vyukov bounded MPMC queue)
 *
 * The concurrent cases are designed to fault under ThreadSanitizer if the
 * memory ordering or ABA handling is wrong, and to detect lost/duplicated
 * elements (which a torn 16-byte CAS or an ABA bug would produce). */
#include "scl_test.h"
#include "scl_atomic.h"
#include "scl_concurrent_common.h"
#include <stdatomic.h>

/* ── Single-threaded DWCAS sanity ───────────────────────────── */
static void test_dwcas_basic(scl_test_runner_t *tr) {
    scl_test_group("DWCAS: single-threaded semantics");

    _Alignas(16) volatile scl_dwcas_t cell = { .lo = 1, .hi = 2 };

    scl_dwcas_t expected = { .lo = 1, .hi = 2 };
    scl_dwcas_t desired  = { .lo = 42, .hi = 99 };
    SCL_EXPECT_TRUE(tr, scl_dwcas(&cell, &expected, desired));      /* matches -> swap */
    SCL_EXPECT_EQ_U(tr, cell.lo, 42);
    SCL_EXPECT_EQ_U(tr, cell.hi, 99);

    scl_dwcas_t wrong = { .lo = 0, .hi = 0 };
    scl_dwcas_t other = { .lo = 7, .hi = 7 };
    SCL_EXPECT_FALSE(tr, scl_dwcas(&cell, &wrong, other));          /* mismatch -> no swap */
    SCL_EXPECT_EQ_U(tr, wrong.lo, 42);   /* expected updated to observed */
    SCL_EXPECT_EQ_U(tr, wrong.hi, 99);
    SCL_EXPECT_EQ_U(tr, cell.lo, 42);    /* memory unchanged */

    scl_dwcas_t loaded = scl_dwcas_load(&cell);
    SCL_EXPECT_EQ_U(tr, loaded.lo, 42);
    SCL_EXPECT_EQ_U(tr, loaded.hi, 99);
}

/* ── Lock-free stack ────────────────────────────────────────── */
typedef struct {
    uintptr_t next;          /* MUST be first: the stack link */
    int       id;
    atomic_int held;         /* detects two threads owning the same node */
} lf_node_t;

#define LF_NODES 64
#define LF_ITERS 20000

typedef struct {
    scl_lfstack_t stack;
    lf_node_t     nodes[LF_NODES];
} lf_ctx_t;

static void *lf_stack_worker(void *p) {
    scl_test_thread_arg_t *arg = (scl_test_thread_arg_t *)p;
    lf_ctx_t *ctx = (lf_ctx_t *)arg->user_data;

    scl_test_barrier_wait(arg->barrier);
    for (int i = 0; i < LF_ITERS; i++) {
        lf_node_t *n = (lf_node_t *)scl_lfstack_pop(&ctx->stack);
        if (!n) continue;                          /* contended empty: retry */
        /* If ABA/tearing handed this node to two threads, held would already
         * be 1 when we acquire it. */
        int prev = atomic_exchange_explicit(&n->held, 1, memory_order_acq_rel);
        SCL_CC_EXPECT(arg->cc, prev == 0);
        atomic_store_explicit(&n->held, 0, memory_order_release);
        scl_lfstack_push(&ctx->stack, n);
    }
    return NULL;
}

static void test_lfstack_concurrent(scl_test_runner_t *tr) {
    scl_test_group("LF stack: concurrent push/pop conserves nodes");

    lf_ctx_t *ctx = (lf_ctx_t *)calloc(1, sizeof(lf_ctx_t));
    SCL_EXPECT_NOT_NULL(tr, ctx);
    if (!ctx) return;

    scl_lfstack_init(&ctx->stack);
    for (int i = 0; i < LF_NODES; i++) {
        ctx->nodes[i].id = i;
        atomic_init(&ctx->nodes[i].held, 0);
        scl_lfstack_push(&ctx->stack, &ctx->nodes[i]);
    }

    scl_test_run_concurrent(tr, 8, lf_stack_worker, ctx);

    /* Drain: every node must come back exactly once. */
    int seen[LF_NODES] = {0};
    int count = 0;
    lf_node_t *n;
    while ((n = (lf_node_t *)scl_lfstack_pop(&ctx->stack)) != NULL) {
        SCL_EXPECT_TRUE(tr, n->id >= 0 && n->id < LF_NODES);
        if (n->id >= 0 && n->id < LF_NODES) seen[n->id]++;
        count++;
    }
    SCL_EXPECT_EQ_I(tr, count, LF_NODES);
    int dupes = 0, missing = 0;
    for (int i = 0; i < LF_NODES; i++) {
        if (seen[i] > 1) dupes++;
        if (seen[i] == 0) missing++;
    }
    SCL_EXPECT_EQ_I(tr, dupes, 0);
    SCL_EXPECT_EQ_I(tr, missing, 0);
    free(ctx);
}

/* ── MPMC queue ─────────────────────────────────────────────── */
#define MPMC_PRODUCERS 4
#define MPMC_PER       50000
#define MPMC_TOTAL     (MPMC_PRODUCERS * MPMC_PER)

typedef struct {
    scl_mpmc_queue_t q;
    atomic_int       produced;
    atomic_int       consumed;
    atomic_uchar     seen[MPMC_TOTAL];
} mpmc_ctx_t;

/* First half of threads produce, second half consume. */
static void *mpmc_worker(void *p) {
    scl_test_thread_arg_t *arg = (scl_test_thread_arg_t *)p;
    mpmc_ctx_t *ctx = (mpmc_ctx_t *)arg->user_data;
    int half = arg->nthreads / 2;
    scl_test_barrier_wait(arg->barrier);

    if (arg->thread_id < half) {
        /* producer: claims a disjoint slice of the value space */
        int pid = arg->thread_id;
        for (int i = 0; i < MPMC_PER; i++) {
            uintptr_t v = (uintptr_t)(pid * MPMC_PER + i) + 1;  /* +1: never 0 */
            while (!scl_mpmc_enqueue(&ctx->q, v))
                scl_cpu_pause();                                /* full: spin */
            atomic_fetch_add_explicit(&ctx->produced, 1, memory_order_relaxed);
        }
    } else {
        /* consumer: dequeue until all producers are done and queue drained */
        for (;;) {
            uintptr_t v;
            if (scl_mpmc_dequeue(&ctx->q, &v)) {
                int idx = (int)(v - 1);
                SCL_CC_EXPECT(arg->cc, idx >= 0 && idx < MPMC_TOTAL);
                if (idx >= 0 && idx < MPMC_TOTAL) {
                    unsigned char prev = atomic_exchange_explicit(
                        &ctx->seen[idx], 1, memory_order_relaxed);
                    SCL_CC_EXPECT(arg->cc, prev == 0);   /* no duplicate delivery */
                }
                atomic_fetch_add_explicit(&ctx->consumed, 1, memory_order_relaxed);
            } else {
                if (atomic_load_explicit(&ctx->produced, memory_order_acquire) == MPMC_TOTAL &&
                    atomic_load_explicit(&ctx->consumed, memory_order_acquire) == MPMC_TOTAL)
                    break;
                scl_cpu_pause();
            }
        }
    }
    return NULL;
}

static void test_mpmc_concurrent(scl_test_runner_t *tr) {
    scl_test_group("MPMC queue: every item delivered exactly once");

    mpmc_ctx_t *ctx = (mpmc_ctx_t *)calloc(1, sizeof(mpmc_ctx_t));
    SCL_EXPECT_NOT_NULL(tr, ctx);
    if (!ctx) return;

    SCL_EXPECT_OK(tr, scl_mpmc_init(scl_allocator_default(), &ctx->q, 1024));
    atomic_init(&ctx->produced, 0);
    atomic_init(&ctx->consumed, 0);
    for (int i = 0; i < MPMC_TOTAL; i++) atomic_init(&ctx->seen[i], 0);

    scl_test_run_concurrent(tr, MPMC_PRODUCERS * 2, mpmc_worker, ctx);

    SCL_EXPECT_EQ_I(tr, atomic_load(&ctx->consumed), MPMC_TOTAL);
    int missing = 0;
    for (int i = 0; i < MPMC_TOTAL; i++)
        if (atomic_load_explicit(&ctx->seen[i], memory_order_relaxed) == 0) missing++;
    SCL_EXPECT_EQ_I(tr, missing, 0);

    scl_mpmc_destroy(&ctx->q);
    free(ctx);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_dwcas_basic(&tr);
    test_lfstack_concurrent(&tr);
    test_mpmc_concurrent(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
