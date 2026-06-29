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

/* Mergesort. O(N log N). Stable. Top-down or bottom-up. O(N) auxiliary space. */

#include "scl_merge.h"
#include "scl_string.h"
#include "scl_pthread.h"

#include <stdbool.h>
#include <unistd.h>   /* sysconf(_SC_NPROCESSORS_ONLN) */

scl_error_t scl_sort_merge_sort(scl_allocator_t *alloc, void * base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (scl_unlikely(!base || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count < 2)) return SCL_OK;

    size_t bytes;
    if (scl_unlikely(scl_mul_overflow(count, element_size, &bytes)))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t width = 1; scl_likely(width < count); width *= 2) {
        for (size_t i = 0; i < count; i += 2 * width) {
            size_t mid = i + width;
            if (scl_unlikely(mid >= count)) continue;
            size_t r = i + 2 * width;
            if (r > count) r = count;

            size_t li = i, ri = mid, ti = 0;

            while (scl_likely(li < mid && ri < r)) {
                if (cmp((unsigned char *)base + li * element_size,
                        (unsigned char *)base + ri * element_size) <= 0)
                    scl_memcpy(tmp + ti++ * element_size,
                           (unsigned char *)base + li++ * element_size,
                           element_size);
                else
                    scl_memcpy(tmp + ti++ * element_size,
                           (unsigned char *)base + ri++ * element_size,
                           element_size);
            }
            while (li < mid)
                scl_memcpy(tmp + ti++ * element_size,
                       (unsigned char *)base + li++ * element_size,
                       element_size);
            while (ri < r)
                scl_memcpy(tmp + ti++ * element_size,
                       (unsigned char *)base + ri++ * element_size,
                       element_size);

            scl_memcpy((unsigned char *)base + i * element_size, tmp, ti * element_size);
        }
    }

    scl_free(alloc, tmp);
    return SCL_OK;
}

/* ── Multithreaded mergesort ──────────────────────────────────────────────
 *
 * Top-down parallel divide-and-conquer. Each subtask owns a disjoint index
 * range [lo, hi) of both `base` and the shared scratch buffer `tmp`, so sibling
 * tasks never touch the same memory — no locking and no data races. A subtask
 * is offloaded to a new thread only while a depth budget (derived from the
 * thread cap) remains and the range is large enough to be worth a thread;
 * otherwise it runs inline. The right half always runs in the current thread
 * while the left half may run in a spawned one, bounding live threads.
 */

/* Below this many elements, parallelism overhead outweighs the benefit. */
#define SCL_MSORT_PAR_CUTOFF 2048

/* Merge sorted [lo,mid) and [mid,hi) (in base) via tmp, writing back to base.
 * Stable: equal elements keep left-before-right order. */
static void msort_merge(unsigned char *base, size_t lo, size_t mid, size_t hi,
                        unsigned char *tmp, size_t es, scl_cmp_func_t cmp)
{
    size_t li = lo, ri = mid, ti = lo;
    while (li < mid && ri < hi) {
        if (cmp(base + li * es, base + ri * es) <= 0)
            scl_memcpy(tmp + ti++ * es, base + li++ * es, es);
        else
            scl_memcpy(tmp + ti++ * es, base + ri++ * es, es);
    }
    while (li < mid) scl_memcpy(tmp + ti++ * es, base + li++ * es, es);
    while (ri < hi)  scl_memcpy(tmp + ti++ * es, base + ri++ * es, es);
    scl_memcpy(base + lo * es, tmp + lo * es, (hi - lo) * es);
}

/* Sequential top-down sort of [lo,hi). */
static void msort_seq(unsigned char *base, size_t lo, size_t hi,
                      unsigned char *tmp, size_t es, scl_cmp_func_t cmp)
{
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    msort_seq(base, lo, mid, tmp, es, cmp);
    msort_seq(base, mid, hi, tmp, es, cmp);
    msort_merge(base, lo, mid, hi, tmp, es, cmp);
}

typedef struct {
    unsigned char  *base;
    size_t          lo, hi;
    unsigned char  *tmp;
    size_t          es;
    scl_cmp_func_t  cmp;
    int             depth;
} msort_ctx_t;

static void msort_par(unsigned char *base, size_t lo, size_t hi,
                      unsigned char *tmp, size_t es, scl_cmp_func_t cmp, int depth);

static void *msort_thread(void *p)
{
    msort_ctx_t *c = (msort_ctx_t *)p;
    msort_par(c->base, c->lo, c->hi, c->tmp, c->es, c->cmp, c->depth);
    return NULL;
}

static void msort_par(unsigned char *base, size_t lo, size_t hi,
                      unsigned char *tmp, size_t es, scl_cmp_func_t cmp, int depth)
{
    size_t n = hi - lo;
    if (n < 2) return;
    if (depth <= 0 || n < SCL_MSORT_PAR_CUTOFF) {
        msort_seq(base, lo, hi, tmp, es, cmp);
        return;
    }

    size_t mid = lo + n / 2;

    /* Offload the left half to a worker; the ctx lives on this stack frame and
     * is valid until we join below. */
    msort_ctx_t lc = { base, lo, mid, tmp, es, cmp, depth - 1 };
    scl_thread_t th;
    bool spawned = (scl_thread_create(&th, msort_thread, &lc) == SCL_OK);
    if (!spawned)
        msort_par(base, lo, mid, tmp, es, cmp, 0);  /* thread exhaustion: inline */

    msort_par(base, mid, hi, tmp, es, cmp, depth - 1);

    if (spawned)
        (void)scl_thread_join(th, NULL);

    msort_merge(base, lo, mid, hi, tmp, es, cmp);
}

scl_error_t scl_sort_merge_sort_mt(scl_allocator_t *alloc, void *base, size_t count,
                                   size_t element_size, scl_cmp_func_t cmp,
                                   unsigned int max_threads)
{
    if (scl_unlikely(!base || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(count < 2)) return SCL_OK;

    size_t bytes;
    if (scl_unlikely(scl_mul_overflow(count, element_size, &bytes)))
        return SCL_ERR_SIZE_OVERFLOW;

    /* Resolve and clamp the thread budget. */
    unsigned int threads = max_threads;
    if (threads == 0) {
        long online = sysconf(_SC_NPROCESSORS_ONLN);
        threads = (online > 0) ? (unsigned int)online : 1u;
    }
    if (threads > SCL_SORT_MERGE_MAX_THREADS) threads = SCL_SORT_MERGE_MAX_THREADS;
    if (threads < 1) threads = 1;

    /* Depth budget: each level may spawn one thread, so log2(threads) levels of
     * splitting reach ~`threads` concurrent leaves. */
    int depth = 0;
    for (unsigned int t = 1; t < threads; t <<= 1) depth++;

    unsigned char *tmp = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;

    if (depth == 0)
        msort_seq((unsigned char *)base, 0, count, tmp, element_size, cmp);
    else
        msort_par((unsigned char *)base, 0, count, tmp, element_size, cmp, depth);

    scl_free(alloc, tmp);
    return SCL_OK;
}
