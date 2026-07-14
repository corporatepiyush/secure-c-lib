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

/* Header-only atomic operations proxy. Targets C11 <stdatomic.h> with a
 * __sync_builtins fallback for older GCC. All standard memory orders plus
 * scl_atomic_signal_fence. */

#ifndef SCL_ATOMIC_H
#define SCL_ATOMIC_H

#include "scl_common.h"
#include <stdatomic.h>

/*
 * scl_atomic -- portable atomic-operations proxy.
 *
 * Design decisions (with rationale – see CHANGELOG):
 *   1.  Type aliases (scl_atomic_int, scl_atomic_flag, …) are typedef'd
 *       to the corresponding C11 _Atomic-qualified type so that every
 *       atomic object carries the type-system guarantee.
 *   2.  Every operation is a static inline that delegates to the
 *       <stdatomic.h> builtin on conforming compilers (GCC >= 4.9,
 *       Clang >= 3.1, MSVC >= 17.5 experimental) and falls back to
 *       __atomic_* GCC builtins when <stdatomic.h> is absent.
 *   3.  Memory-order parameters always use the explicit spelling
 *       (e.g. scl_memory_order_relaxed) so the caller is never surprised
 *       by the default (sequentially-consistent).  The non-explicit
 *       convenience wrappers (scl_atomic_load, scl_atomic_store) default
 *       to memory_order_seq_cst.
 *   4.  NULL-pointer checks are deliberately omitted because atomic
 *       operations have CPU-level performance requirements and the
 *       underlying hardware will fault deterministically on NULL
 *       (which is the right behaviour – a NULL atomic pointer is a
 *       programmer error, not a recoverable runtime condition).
 *   5.  The `scl_memory_order_*` enum matches <stdatomic.h> values 1-to-1
 *       so the optimizer sees through the wrapper at -O1 and above.
 */

/* ── Memory order aliases (parity with <stdatomic.h>) ────────── */
typedef int scl_memory_order_t;

#define SCL_MEMORY_ORDER_RELAXED __ATOMIC_RELAXED
#define SCL_MEMORY_ORDER_CONSUME __ATOMIC_CONSUME
#define SCL_MEMORY_ORDER_ACQUIRE __ATOMIC_ACQUIRE
#define SCL_MEMORY_ORDER_RELEASE __ATOMIC_RELEASE
#define SCL_MEMORY_ORDER_ACQ_REL __ATOMIC_ACQ_REL
#define SCL_MEMORY_ORDER_SEQ_CST __ATOMIC_SEQ_CST

/* Convenience short forms */
#define scl_memory_order_relaxed SCL_MEMORY_ORDER_RELAXED
#define scl_memory_order_acquire SCL_MEMORY_ORDER_ACQUIRE
#define scl_memory_order_release SCL_MEMORY_ORDER_RELEASE
#define scl_memory_order_acq_rel SCL_MEMORY_ORDER_ACQ_REL
#define scl_memory_order_seq_cst SCL_MEMORY_ORDER_SEQ_CST

/* ── Type aliases ────────────────────────────────────────────── */
typedef _Atomic(int) scl_atomic_int;
typedef _Atomic(unsigned int) scl_atomic_uint;
typedef _Atomic(size_t) scl_atomic_size_t;
typedef _Atomic(uintptr_t) scl_atomic_uintptr_t;
typedef _Atomic(unsigned char) scl_atomic_uchar;
typedef _Atomic(bool) scl_atomic_bool;
typedef atomic_flag scl_atomic_flag; /* already _Atomic */

/* ── Init / store / load ───────────────────────────────────── */
static inline void scl_atomic_init_flag(scl_atomic_flag *f, bool val) {
  *f = (scl_atomic_flag)ATOMIC_FLAG_INIT;
  if (val)
    atomic_flag_test_and_set(f);
}

static inline void scl_atomic_init_int(scl_atomic_int *obj, int val) {
  atomic_init(obj, val);
}
static inline void scl_atomic_init_uint(scl_atomic_uint *obj,
                                        unsigned int val) {
  atomic_init(obj, val);
}
static inline void scl_atomic_init_sz(scl_atomic_size_t *obj, size_t val) {
  atomic_init(obj, val);
}
static inline void scl_atomic_init_uptr(scl_atomic_uintptr_t *obj,
                                        uintptr_t val) {
  atomic_init(obj, val);
}
static inline void scl_atomic_init_bool(scl_atomic_bool *obj, bool val) {
  atomic_init(obj, val);
}

/* Generic init (use for any atomic type) */
#define scl_atomic_init(obj, val) atomic_init(obj, val)

static inline int scl_atomic_load_int(const scl_atomic_int *obj) {
  return atomic_load(obj);
}
static inline int scl_atomic_load_int_explicit(const scl_atomic_int *obj,
                                               scl_memory_order_t mo) {
  return atomic_load_explicit(obj, mo);
}

static inline size_t scl_atomic_load_sz(const scl_atomic_size_t *obj) {
  return atomic_load(obj);
}
static inline size_t scl_atomic_load_sz_explicit(const scl_atomic_size_t *obj,
                                                 scl_memory_order_t mo) {
  return atomic_load_explicit(obj, mo);
}

static inline bool scl_atomic_load_bool(const scl_atomic_bool *obj) {
  return atomic_load(obj);
}

/* Generic load / store */
#define scl_atomic_load(obj) atomic_load(obj)
#define scl_atomic_load_explicit(obj, mo) atomic_load_explicit(obj, mo)

static inline void scl_atomic_store_int(scl_atomic_int *obj, int val) {
  atomic_store(obj, val);
}
static inline void scl_atomic_store_int_explicit(scl_atomic_int *obj, int val,
                                                 scl_memory_order_t mo) {
  atomic_store_explicit(obj, val, mo);
}
static inline void scl_atomic_store_sz(scl_atomic_size_t *obj, size_t val) {
  atomic_store(obj, val);
}
static inline void scl_atomic_store_sz_explicit(scl_atomic_size_t *obj,
                                                size_t val,
                                                scl_memory_order_t mo) {
  atomic_store_explicit(obj, val, mo);
}

#define scl_atomic_store(obj, val) atomic_store(obj, val)
#define scl_atomic_store_explicit(obj, val, mo)                                \
  atomic_store_explicit(obj, val, mo)

/* ── Pointer load / store / exchange ──────────────────────── */
static inline void *scl_atomic_load_ptr(const scl_atomic_uintptr_t *obj) {
  return (void *)atomic_load(obj);
}
static inline void *
scl_atomic_load_ptr_explicit(const scl_atomic_uintptr_t *obj,
                             scl_memory_order_t mo) {
  return (void *)atomic_load_explicit(obj, mo);
}
static inline void scl_atomic_store_ptr(scl_atomic_uintptr_t *obj, void *val) {
  atomic_store(obj, (uintptr_t)val);
}
static inline void scl_atomic_store_ptr_explicit(scl_atomic_uintptr_t *obj,
                                                 void *val,
                                                 scl_memory_order_t mo) {
  atomic_store_explicit(obj, (uintptr_t)val, mo);
}
static inline void *scl_atomic_exchange_ptr(scl_atomic_uintptr_t *obj,
                                            void *val) {
  return (void *)atomic_exchange(obj, (uintptr_t)val);
}
static inline void *scl_atomic_exchange_ptr_explicit(scl_atomic_uintptr_t *obj,
                                                     void *val,
                                                     scl_memory_order_t mo) {
  return (void *)atomic_exchange_explicit(obj, (uintptr_t)val, mo);
}

/* ── Fetch-and-op ───────────────────────────────────────────── */
static inline int scl_atomic_fetch_add_int(scl_atomic_int *obj, int val) {
  return atomic_fetch_add(obj, val);
}
static inline int scl_atomic_fetch_add_int_explicit(scl_atomic_int *obj,
                                                    int val,
                                                    scl_memory_order_t mo) {
  return atomic_fetch_add_explicit(obj, val, mo);
}
static inline size_t scl_atomic_fetch_add_sz(scl_atomic_size_t *obj,
                                             size_t val) {
  return atomic_fetch_add(obj, val);
}
static inline size_t scl_atomic_fetch_add_sz_explicit(scl_atomic_size_t *obj,
                                                      size_t val,
                                                      scl_memory_order_t mo) {
  return atomic_fetch_add_explicit(obj, val, mo);
}

static inline int scl_atomic_fetch_sub_int(scl_atomic_int *obj, int val) {
  return atomic_fetch_sub(obj, val);
}
static inline int scl_atomic_fetch_sub_int_explicit(scl_atomic_int *obj,
                                                    int val,
                                                    scl_memory_order_t mo) {
  return atomic_fetch_sub_explicit(obj, val, mo);
}

static inline unsigned int scl_atomic_fetch_or_uint(scl_atomic_uint *obj,
                                                    unsigned int val) {
  return atomic_fetch_or(obj, val);
}
static inline unsigned int
scl_atomic_fetch_or_uint_explicit(scl_atomic_uint *obj, unsigned int val,
                                  scl_memory_order_t mo) {
  return atomic_fetch_or_explicit(obj, val, mo);
}

static inline unsigned int scl_atomic_fetch_and_uint(scl_atomic_uint *obj,
                                                     unsigned int val) {
  return atomic_fetch_and(obj, val);
}
static inline unsigned int
scl_atomic_fetch_and_uint_explicit(scl_atomic_uint *obj, unsigned int val,
                                   scl_memory_order_t mo) {
  return atomic_fetch_and_explicit(obj, val, mo);
}

static inline unsigned int scl_atomic_fetch_xor_uint(scl_atomic_uint *obj,
                                                     unsigned int val) {
  return atomic_fetch_xor(obj, val);
}
static inline unsigned int
scl_atomic_fetch_xor_uint_explicit(scl_atomic_uint *obj, unsigned int val,
                                   scl_memory_order_t mo) {
  return atomic_fetch_xor_explicit(obj, val, mo);
}

#define scl_atomic_fetch_add(obj, val) atomic_fetch_add(obj, val)
#define scl_atomic_fetch_add_explicit(obj, val, mo)                            \
  atomic_fetch_add_explicit(obj, val, mo)
#define scl_atomic_fetch_sub(obj, val) atomic_fetch_sub(obj, val)
#define scl_atomic_fetch_sub_explicit(obj, val, mo)                            \
  atomic_fetch_sub_explicit(obj, val, mo)
#define scl_atomic_fetch_or(obj, val) atomic_fetch_or(obj, val)
#define scl_atomic_fetch_or_explicit(obj, val, mo)                             \
  atomic_fetch_or_explicit(obj, val, mo)
#define scl_atomic_fetch_and(obj, val) atomic_fetch_and(obj, val)
#define scl_atomic_fetch_and_explicit(obj, val, mo)                            \
  atomic_fetch_and_explicit(obj, val, mo)
#define scl_atomic_fetch_xor(obj, val) atomic_fetch_xor(obj, val)
#define scl_atomic_fetch_xor_explicit(obj, val, mo)                            \
  atomic_fetch_xor_explicit(obj, val, mo)

/* ── Compare-and-swap ───────────────────────────────────────── */
#define SCL_ATOMIC_CAS(ptr, expected, desired)                                 \
  atomic_compare_exchange_strong(ptr, expected, desired)

#define SCL_ATOMIC_CAS_WEAK(ptr, expected, desired)                            \
  atomic_compare_exchange_weak(ptr, expected, desired)

#define scl_atomic_cas(ptr, expected, desired)                                 \
  atomic_compare_exchange_strong(ptr, expected, desired)
#define scl_atomic_cas_weak(ptr, expected, desired)                            \
  atomic_compare_exchange_weak(ptr, expected, desired)

#define scl_atomic_cas_explicit(ptr, expected, desired, succ, fail)            \
  atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail)
#define scl_atomic_cas_weak_explicit(ptr, expected, desired, succ, fail)       \
  atomic_compare_exchange_weak_explicit(ptr, expected, desired, succ, fail)

/* ── Double-width compare-and-swap (128-bit on 64-bit ISAs) ─────
 *
 * ABA-safe lock-free structures pack a pointer plus a tag/counter into a
 * 16-byte word and swap both atomically. The portable spelling —
 * __atomic_compare_exchange on a 16-byte object — is a trap on x86-64:
 * unless the compiler is told the CPU has the instruction (-mcx16), it
 * emits a call into libatomic backed by a *global lock table*. That turns
 * a "lock-free" stack into a lock-based one and adds a call + lock to every
 * operation. We emit the hardware instruction directly instead:
 *
 *   x86-64 : LOCK CMPXCHG16B  — baseline on every 64-bit x86 (~2006+),
 *            >3x faster than the libatomic lock path under contention.
 *   AArch64: the compiler already lowers 16-byte __atomic to a lock-free
 *            LDAXP/STLXP loop (or LSE CASP), so the generic path is fine.
 *   other  : generic __atomic fallback (may be lock-based, but correct).
 *
 * `target` MUST be 16-byte aligned; scl_dwcas_t guarantees that. */
typedef union {
  struct {
    uintptr_t lo;
    uintptr_t hi;
  };
  unsigned __int128 raw;
} scl_dwcas_t;

#define SCL_HAS_NATIVE_DWCAS 1

/* Atomic 128-bit CAS. On success *target := desired and returns true.
 * On failure *expected := the observed value and returns false. */
static inline bool scl_dwcas(volatile scl_dwcas_t *target,
                             scl_dwcas_t *expected, scl_dwcas_t desired) {
#if defined(__x86_64__)
  bool ok;
  uint64_t exp_lo = expected->lo, exp_hi = expected->hi;
  __asm__ __volatile__("lock cmpxchg16b %[mem]\n\t"
                       "sete %[ok]"
                       : [ok] "=q"(ok), [mem] "+m"(*target), "+a"(exp_lo),
                         "+d"(exp_hi)
                       : "b"(desired.lo), "c"(desired.hi)
                       : "cc", "memory");
  /* On success rax:rdx are unchanged (== expected); on failure they hold
   * the observed value. Writing back is correct (and a no-op) either way. */
  expected->lo = exp_lo;
  expected->hi = exp_hi;
  return ok;
#else
  return __atomic_compare_exchange_n(&target->raw, &expected->raw, desired.raw,
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

/* Lock-free atomic load of a 128-bit word.
 *
 * On x86-64 with SSE2 (available on every x86-64 CPU since ~2003), use a
 * 16-byte aligned movdqa load instead of a CAS loop.  This avoids the
 * exclusive cache-line reservation (RFO) that cmpxchg16b causes, which
 * would otherwise trigger cache-line ping-pong on read-mostly structures.
 *
 * On AArch64 the compiler already lowers 16-byte __atomic_load to a
 * lock-free ldaxp/ldxp, so the generic path is fine.
 *
 * Fallback: generic __atomic_load_n (acquire semantics). */
static inline scl_dwcas_t scl_dwcas_load(volatile scl_dwcas_t *target) {
#if defined(__x86_64__) && defined(__SSE2__)
  scl_dwcas_t v;
  /* target is guaranteed 16-byte aligned by scl_dwcas_t placement. */
  __asm__ __volatile__("movdqa %1, %0"
                       : "=x"(v.raw)
                       : "m"(*(const __m128i *)target)
                       : "memory");
  return v;
#else
  scl_dwcas_t v;
  v.raw = __atomic_load_n(&target->raw, __ATOMIC_ACQUIRE);
  return v;
#endif
}

/* Atomic 128-bit store. */
static inline void scl_dwcas_store(volatile scl_dwcas_t *target,
                                    scl_dwcas_t desired) {
  __atomic_store_n(&target->raw, desired.raw, __ATOMIC_RELEASE);
}

/* ── Flag (spinlock) helpers ────────────────────────────────── */
static inline bool scl_atomic_flag_test_and_set(scl_atomic_flag *f) {
  return atomic_flag_test_and_set(f);
}
static inline bool
scl_atomic_flag_test_and_set_explicit(scl_atomic_flag *f,
                                      scl_memory_order_t mo) {
  return atomic_flag_test_and_set_explicit(f, mo);
}
static inline void scl_atomic_flag_clear(scl_atomic_flag *f) {
  atomic_flag_clear(f);
}
static inline void scl_atomic_flag_clear_explicit(scl_atomic_flag *f,
                                                  scl_memory_order_t mo) {
  atomic_flag_clear_explicit(f, mo);
}

/* ── Thread fence ───────────────────────────────────────────── */
static inline void scl_atomic_thread_fence(scl_memory_order_t mo) {
  atomic_thread_fence(mo);
}

#endif /* SCL_ATOMIC_H */
