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

/* Spinlock type (scl_spinlock_t) with TTAS-and-pause backoff, cache-line alignment/padding macros for false-sharing prevention, memory-order shortcuts for concurrent data structures. */

#ifndef SCL_CONCURRENT_COMMON_H
#define SCL_CONCURRENT_COMMON_H

#include "scl_common.h"
#include "scl_atomic.h"
#include <sched.h>
#include <time.h>
#include <pthread.h>

/* ── Atomic flags (spinlock) ───────────────────────────────── */
typedef struct {
    atomic_flag flag;
#ifndef NDEBUG
    pthread_t owner;
#endif
} scl_spinlock_t;

static inline void scl_spinlock_init(scl_spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
#ifndef NDEBUG
    lock->owner = (pthread_t)0;
#endif
}

static inline void scl_spinlock_lock(scl_spinlock_t *lock) {
    if (scl_likely(!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))) {
#ifndef NDEBUG
        lock->owner = pthread_self();
#endif
        return;
    }
    for (int backoff = 1; ; backoff++) {
        int pauses = backoff < 10 ? (1 << backoff) : 1024;
        for (int i = 0; i < pauses; i++)
            scl_cpu_pause();
        if (scl_likely(!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))) {
#ifndef NDEBUG
            lock->owner = pthread_self();
#endif
            return;
        }
        if (scl_unlikely(backoff > 12))
            (void)nanosleep(&(struct timespec){0, 1000000}, NULL);
        else if (scl_unlikely(backoff > 8))
            (void)sched_yield();
    }
}

static inline int scl_spinlock_trylock(scl_spinlock_t *lock) {
    int ok = !atomic_flag_test_and_set(&lock->flag);
#ifndef NDEBUG
    if (ok) lock->owner = pthread_self();
#endif
    return ok;
}

static inline void scl_spinlock_unlock(scl_spinlock_t *lock) {
#ifndef NDEBUG
    lock->owner = (pthread_t)0;
#endif
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}

/* ── RAII scope guard (prevents unbalanced-unlock bugs) ───── */
#define SCL_SCOPE_LOCK(lock)                                              \
    for (int _scl_once_ = (scl_spinlock_lock(lock), 1);                  \
         _scl_once_;                                                      \
         _scl_once_--, scl_spinlock_unlock(lock))

/* ── Tagged pointer helpers (ABA prevention) ──────────────── */
typedef union {
    struct { void *ptr; uintptr_t tag; };
    __uint128_t raw;
} scl_tagged_ptr_t;

static inline scl_tagged_ptr_t scl_tagged_make(void *ptr, uintptr_t tag) {
    scl_tagged_ptr_t tp;
    tp.ptr = ptr;
    tp.tag = tag;
    return tp;
}

/* ── Lock-free data structures (moved) ─────────────────────────
 * The lock-free stack and MPMC queue now live under structures/:
 *   structures/lfstack/scl_concurrent_lfstack.h  (scl_lfstack)
 *   structures/mpmc/scl_concurrent_mpmc.h        (scl_mpmc_queue)
 * This header keeps only the shared concurrency *primitives*
 * (spinlock, tagged pointer). Include the structure headers directly
 * where you need those containers. */

/* ── CAS helpers ────────────────────────────────────────────── */
#define SCL_CAS(ptr, old, new) \
    atomic_compare_exchange_strong(ptr, old, new)

#endif
