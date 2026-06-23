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
    if (!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))
        return;
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

/* ── Lock-free Treiber stack (ABA-safe via tagged ptr) ────── */
typedef scl_tagged_ptr_t scl_treiber_stack_t;

static inline void scl_treiber_init(scl_treiber_stack_t *stack) {
    stack->raw = 0;
}
static inline bool scl_treiber_empty(scl_treiber_stack_t *stack) {
    return stack->ptr == NULL;
}
static inline void scl_treiber_push(scl_treiber_stack_t *stack,
                                    scl_tagged_ptr_t node,
                                    volatile uintptr_t *next_field) {
    scl_tagged_ptr_t old;
    old.raw = __atomic_load_n(&stack->raw, __ATOMIC_RELAXED);
    do {
        node.ptr = (void *)((uintptr_t)node.ptr & ~(uintptr_t)3);
        __atomic_store_n(next_field, (uintptr_t)old.ptr, __ATOMIC_RELEASE);
        old.tag++;
    } while (!__atomic_compare_exchange_n(&stack->raw, &old.raw,
                 (scl_tagged_ptr_t){.ptr = node.ptr, .tag = old.tag}.raw,
                 false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
}
static inline scl_tagged_ptr_t scl_treiber_pop(scl_treiber_stack_t *stack,
                                                volatile uintptr_t *next_field) {
    scl_tagged_ptr_t old;
    old.raw = __atomic_load_n(&stack->raw, __ATOMIC_RELAXED);
    void *next;
    do {
        if (!old.ptr) return scl_tagged_make(NULL, 0);
        next = (void *)__atomic_load_n(next_field, __ATOMIC_ACQUIRE);
        old.tag++;
    } while (!__atomic_compare_exchange_n(&stack->raw, &old.raw,
                 (scl_tagged_ptr_t){.ptr = (void *)((uintptr_t)next & ~(uintptr_t)3), .tag = old.tag}.raw,
                 false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    return scl_tagged_make(old.ptr, old.tag - 1);
}

/* ── CAS helpers ────────────────────────────────────────────── */
#define SCL_CAS(ptr, old, new) \
    atomic_compare_exchange_strong(ptr, old, new)

#endif
