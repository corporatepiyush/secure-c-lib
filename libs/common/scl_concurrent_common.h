#ifndef SCL_CONCURRENT_COMMON_H
#define SCL_CONCURRENT_COMMON_H

#include "scl_common.h"
#include "scl_atomic.h"
#include <sched.h>
#include <time.h>

/* ── Atomic flags (spinlock) ───────────────────────────────── */
typedef struct {
    atomic_flag flag;
} scl_spinlock_t;

static inline void scl_spinlock_init(scl_spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
}

static inline void scl_spinlock_lock(scl_spinlock_t *lock) {
    /* Fast path: one TAS attempt */
    if (!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))
        return;

    /* Slow path: exponential backoff with PAUSE burst, yield, nanosleep */
    for (int backoff = 1; ; backoff++) {
        int pauses = backoff < 10 ? (1 << backoff) : 1024;
        for (int i = 0; i < pauses; i++)
            scl_cpu_pause();

        if (!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))
            return;

        if (backoff > 12)
            (void)nanosleep(&(struct timespec){0, 1000000}, NULL);
        else if (backoff > 8)
            (void)sched_yield();
    }
}

static inline int scl_spinlock_trylock(scl_spinlock_t *lock) {
    return !atomic_flag_test_and_set(&lock->flag);
}

static inline void scl_spinlock_unlock(scl_spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}

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

/* ── CAS helpers ────────────────────────────────────────────── */
#define SCL_CAS(ptr, old, new) \
    atomic_compare_exchange_strong(ptr, old, new)

#endif
