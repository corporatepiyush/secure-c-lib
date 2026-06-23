#ifndef SCL_CONCURRENT_COMMON_H
#define SCL_CONCURRENT_COMMON_H

#include "scl_common.h"
#include "scl_atomic.h"

/* ── Atomic flags (spinlock) ───────────────────────────────── */
typedef struct {
    atomic_flag flag;
} scl_spinlock_t;

static inline void scl_spinlock_init(scl_spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
}

static inline void scl_spinlock_lock(scl_spinlock_t *lock) {
    while (atomic_flag_test_and_set(&lock->flag)) {
        scl_cpu_pause();
    }
}

static inline int scl_spinlock_trylock(scl_spinlock_t *lock) {
    return !atomic_flag_test_and_set(&lock->flag);
}

static inline void scl_spinlock_unlock(scl_spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
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
