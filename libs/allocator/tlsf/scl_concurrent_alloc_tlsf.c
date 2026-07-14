/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Concurrent TLSF (Two-Level Segregated Fit) allocator.
 *
 * 32 first-level (FL) bins x 32 second-level (SL) bins = 1024 free-lists.
 * Each bin owns a scl_spinlock_t and a doubly-linked free-list, so
 * independent bins can be mutated without disturbing one another.  An
 * atomic two-level bitmap (fl_bm + sl_bm[]) gives O(1) best-fit bin
 * location with a single bitmap scan.
 *
 * Coalescing safety
 * -----------------
 * Physical-block metadata (size_flags, prev_phys links, FREE/PREV_FREE
 * flags) is shared across bins: freeing block X may need to remove its
 * physical neighbours from *their* bins, and a neighbour's size (hence
 * its bin index) can change while another thread is coalescing it.
 * Per-bin locks alone cannot protect that metadata without data races on
 * size_flags.  A single pool-level spinlock (pool_lock) therefore
 * serialises the alloc/free/realloc entry points; the per-bin spinlocks
 * still guard every freelist push/pop as required by the design.  This is
 * correct, deadlock-free, and keeps the per-bin structure that future
 * fine-grained work can build on.
 */

#include "scl_concurrent_alloc_tlsf.h"
#include "scl_concurrent_common.h"
#include "scl_string.h"

#include "scl_atomic.h"
#include "scl_stdalign.h"
#include "scl_stddef.h"
#include "scl_stdint.h"

/* ── Constants ─────────────────────────────────────────────── */
#define CTLSF_FL_MAX 32
#define CTLSF_SL_COUNT 32
#define CTLSF_SL_LOG2 5
#define CTLSF_SMALL_BLOCK 256U

/* Block header — user data begins immediately after the FULL header
 * (i.e. at offset sizeof(ctlsf_hdr_t)).  See scl_concurrent_alloc_tlsf.h
 * design notes. */
typedef struct ctlsf_hdr {
  uint32_t size_flags;         /* bits 0..2 flags; rest = block size  */
  struct ctlsf_hdr *prev_phys; /* previous physical block in the pool */
  struct ctlsf_hdr *next_free; /* next block in this bin's freelist  */
  struct ctlsf_hdr *prev_free; /* prev block in this bin's freelist  */
} ctlsf_hdr_t;

/* Flag bits stored in the low 3 bits of size_flags. */
#define CTLSF_FREE_FLAG 1U /* bit 0: block is free                  */
#define CTLSF_PREV_FREE 4U /* bit 2: previous physical block is free */
#define CTLSF_FLAG_MASK 7U
#define CTLSF_SIZE_MASK (~7U)

#define CTLSF_HDR_SZ sizeof(ctlsf_hdr_t)
/* A remnant must at least hold a full free-block header to be useful. */
#define CTLSF_MIN_BLOCK (2u * (uint32_t)sizeof(ctlsf_hdr_t))

/* ── Per-bin descriptor ─────────────────────────────────────── */
typedef struct {
  scl_spinlock_t lock;
  ctlsf_hdr_t *head; /* first free block in this bin */
  uint8_t
      _pad[112]; /* pad to 128 bytes (one cache line on ARM64/modern x86-64) */
} ctlsf_bin_t;

/* ── Allocator state ────────────────────────────────────────── */
typedef struct {
  scl_allocator_t *backing;
  void *pool;
  size_t pool_size;
  unsigned char *pool_end;         /* one-past-end sentinel boundary */
  scl_spinlock_t pool_lock;        /* serialises coalescing metadata  */
  atomic_uint fl_bm;               /* FL occupancy bitmap             */
  atomic_uint sl_bm[CTLSF_FL_MAX]; /* per-FL SL occupancy       */
  ctlsf_bin_t bins[CTLSF_FL_MAX][CTLSF_SL_COUNT];
} ctlsf_state_t;

/* ── Bit helpers ───────────────────────────────────────────── */
static SCL_ALWAYS_INLINE SCL_PURE int ctlsl_fls_size(size_t v) {
  if (v == 0)
    return 0;
  return (int)scl_log2_sz(v);
}

static SCL_ALWAYS_INLINE SCL_PURE int ctlsl_ctz(unsigned int v) {
  return v ? (int)__builtin_ctz(v) : 0;
}

/* ── TLSF size→bin mapping ──────────────────────────────────── */
/* Search mapping (uses XOR): identifies the first bin that can
 * satisfy a request of `size` bytes. */
static SCL_ALWAYS_INLINE void ctlsf_mapping_search(size_t size, int *fl,
                                                   int *sl) {
  if (scl_likely(size < CTLSF_SMALL_BLOCK)) {
    *fl = 0;
    *sl = (int)(size / (CTLSF_SMALL_BLOCK / CTLSF_SL_COUNT));
  } else {
    int s = ctlsl_fls_size(size);
    *fl = s - (CTLSF_SL_LOG2 - 1);
    *sl = (int)((size >> (s - CTLSF_SL_LOG2)) ^ (1u << CTLSF_SL_LOG2));
    if (scl_unlikely(*sl >= CTLSF_SL_COUNT))
      *sl = CTLSF_SL_COUNT - 1;
  }
}

/* Insert mapping (uses subtraction): the exact bin a block of
 * `size` bytes belongs in. */
static SCL_ALWAYS_INLINE void ctlsf_mapping_insert(size_t size, int *fl,
                                                   int *sl) {
  if (scl_likely(size < CTLSF_SMALL_BLOCK)) {
    *fl = 0;
    *sl = (int)(size / (CTLSF_SMALL_BLOCK / CTLSF_SL_COUNT));
  } else {
    int s = ctlsl_fls_size(size);
    *fl = s - (CTLSF_SL_LOG2 - 1);
    *sl = (int)((size >> (s - CTLSF_SL_LOG2)) - (1u << CTLSF_SL_LOG2));
  }
}

/* ── Freelist operations (per-bin spinlock protected) ───────── */

static SCL_ALWAYS_INLINE void ctlsf_bin_push(ctlsf_state_t *st, int fl, int sl,
                                             ctlsf_hdr_t *block) {
  ctlsf_bin_t *bin = &st->bins[fl][sl];
  SCL_SCOPE_LOCK(&bin->lock) {
    block->prev_free = NULL;
    block->next_free = bin->head;
    if (scl_likely(bin->head != NULL))
      bin->head->prev_free = block;
    bin->head = block;
  }
  atomic_fetch_or_explicit(&st->sl_bm[fl], 1u << sl, memory_order_release);
  atomic_fetch_or_explicit(&st->fl_bm, 1u << fl, memory_order_release);
}

static SCL_ALWAYS_INLINE void ctlsf_bin_pop(ctlsf_state_t *st, int fl, int sl,
                                            ctlsf_hdr_t *block) {
  ctlsf_bin_t *bin = &st->bins[fl][sl];
  int empty_after = 0;
  SCL_SCOPE_LOCK(&bin->lock) {
    if (scl_likely(block->prev_free))
      block->prev_free->next_free = block->next_free;
    if (scl_likely(block->next_free))
      block->next_free->prev_free = block->prev_free;
    if (scl_unlikely(bin->head == block))
      bin->head = block->next_free;
    empty_after = (bin->head == NULL);
  }
  block->next_free = NULL;
  block->prev_free = NULL;
  if (scl_unlikely(empty_after)) {
    atomic_fetch_and_explicit(&st->sl_bm[fl], ~(1u << sl),
                              memory_order_release);
    /* Re-read under release to decide whether to clear the FL bit. */
    if (atomic_load_explicit(&st->sl_bm[fl], memory_order_acquire) == 0u)
      atomic_fetch_and_explicit(&st->fl_bm, ~(1u << fl), memory_order_release);
  }
}

/* ── Best-fit bin search (atomic bitmaps, equal-or-higher FL) ─ */
/* Returns 1 and fills out_fl and out_sl on success, 0 if no bin fits. */
static SCL_ALWAYS_INLINE int ctlsf_search(const ctlsf_state_t *st, size_t size,
                                          int *out_fl, int *out_sl) {
  int fl, sl;
  ctlsf_mapping_search(size, &fl, &sl);

  unsigned int sl_mask =
      atomic_load_explicit(&st->sl_bm[fl], memory_order_acquire) >> sl;
  if (sl_mask) {
    sl += ctlsl_ctz(sl_mask);
  } else {
    unsigned int fl_mask =
        atomic_load_explicit(&st->fl_bm, memory_order_acquire) >> (fl + 1);
    if (!fl_mask)
      return 0;
    fl += 1 + ctlsl_ctz(fl_mask);
    sl = ctlsl_ctz(atomic_load_explicit(&st->sl_bm[fl], memory_order_acquire));
  }
  *out_fl = fl;
  *out_sl = sl;
  return 1;
}

/* ── vtable forward decl ────────────────────────────────────── */
static void ctlsf_free_fn(void *state, void *ptr);

/* ── malloc ─────────────────────────────────────────────────── */
static void *ctlsf_malloc_fn(void *state, size_t size, size_t alignment) {
  ctlsf_state_t *st = (ctlsf_state_t *)state;
  if (scl_unlikely(st == NULL || size == 0))
    return NULL;
  (void)alignment; /* pool is max_align_t-aligned; header preserves it */

  /* Round user size up to max_align_t, then add the full header. */
  size_t align = alignof(max_align_t);
  size_t mod = size % align;
  if (mod != 0)
    size += align - mod;

  size_t req;
  if (scl_unlikely(scl_add_overflow(size, CTLSF_HDR_SZ, &req)))
    return NULL;
  if (req < CTLSF_MIN_BLOCK)
    req = CTLSF_MIN_BLOCK;

  void *user = NULL;
  SCL_SCOPE_LOCK(&st->pool_lock) {
    int fl, sl;
    if (scl_unlikely(!ctlsf_search(st, req, &fl, &sl))) {
      /* no bin satisfies the request */
    } else {
      ctlsf_hdr_t *block = NULL;
      int got = 0;
      SCL_SCOPE_LOCK(&st->bins[fl][sl].lock) {
        block = st->bins[fl][sl].head;
        if (scl_likely(block != NULL)) {
          st->bins[fl][sl].head = block->next_free;
          if (block->next_free)
            block->next_free->prev_free = NULL;
          got = 1;
        }
      }
      if (scl_likely(got)) {
        block->next_free = NULL;
        block->prev_free = NULL;

        uint32_t block_size = block->size_flags & CTLSF_SIZE_MASK;
        uint32_t remaining = block_size - (uint32_t)req;

        if (scl_likely(remaining >= CTLSF_MIN_BLOCK)) {
          /* Split: carve the tail off as a new free block. */
          ctlsf_hdr_t *rem = (ctlsf_hdr_t *)((unsigned char *)block + req);
          rem->prev_phys = block;
          rem->size_flags = remaining | CTLSF_FREE_FLAG;
          /* `block` is about to become allocated, so the
           * remnant's PREV_FREE is clear. */
          block->size_flags =
              (uint32_t)req | (block->size_flags & CTLSF_PREV_FREE);

          /* The block after the remnant must learn that its
           * previous physical block (the remnant) is free. */
          ctlsf_hdr_t *after =
              (ctlsf_hdr_t *)((unsigned char *)rem + remaining);
          if (scl_likely((unsigned char *)after < st->pool_end))
            after->size_flags |= CTLSF_PREV_FREE;

          int rfl, rsl;
          ctlsf_mapping_insert(remaining, &rfl, &rsl);
          ctlsf_bin_push(st, rfl, rsl, rem);
        } else {
          /* Use the whole block; no split. */
          block->size_flags =
              block_size | (block->size_flags & CTLSF_PREV_FREE);
          remaining = 0;
        }

        /* The next physical block (after the allocated portion)
         * must have PREV_FREE cleared because `block` is now
         * allocated. */
        ctlsf_hdr_t *next_phys =
            (ctlsf_hdr_t *)((unsigned char *)block +
                            (block->size_flags & CTLSF_SIZE_MASK));
        if (scl_likely((unsigned char *)next_phys < st->pool_end))
          next_phys->size_flags &= ~CTLSF_PREV_FREE;

        /* Clear the FREE flag on the allocated block. */
        block->size_flags &= ~CTLSF_FREE_FLAG;

        /* Refresh the SL/FL bitmaps for the popped bin. */
        int empty = 0;
        SCL_SCOPE_LOCK(&st->bins[fl][sl].lock) {
          empty = (st->bins[fl][sl].head == NULL);
        }
        if (scl_unlikely(empty)) {
          atomic_fetch_and_explicit(&st->sl_bm[fl], ~(1u << sl),
                                    memory_order_release);
          if (atomic_load_explicit(&st->sl_bm[fl], memory_order_acquire) == 0u)
            atomic_fetch_and_explicit(&st->fl_bm, ~(1u << fl),
                                      memory_order_release);
        }

        user = (void *)((unsigned char *)block + CTLSF_HDR_SZ);
      }
    }
  }
  return user;
}

/* ── calloc ────────────────────────────────────────────────── */
static void *ctlsf_calloc_fn(void *state, size_t count, size_t size,
                             size_t alignment) {
  size_t total;
  if (scl_unlikely(scl_mul_overflow(count, size, &total)))
    return NULL;
  void *p = ctlsf_malloc_fn(state, total, alignment);
  if (scl_likely(p != NULL))
    scl_memset(p, 0, total);
  return p;
}

/* ── realloc ───────────────────────────────────────────────── */
static void *ctlsf_realloc_fn(void *state, void *ptr, size_t old_size,
                              size_t new_size, size_t alignment) {
  if (scl_unlikely(ptr == NULL))
    return ctlsf_malloc_fn(state, new_size, alignment);
  if (scl_unlikely(new_size == 0)) {
    ctlsf_free_fn(state, ptr);
    return NULL;
  }
  void *np = ctlsf_malloc_fn(state, new_size, alignment);
  if (scl_likely(np != NULL)) {
    size_t copy = old_size < new_size ? old_size : new_size;
    scl_memcpy(np, ptr, copy);
    ctlsf_free_fn(state, ptr);
  }
  return np;
}

/* ── free ──────────────────────────────────────────────────── */
static void ctlsf_free_fn(void *state, void *ptr) {
  ctlsf_state_t *st = (ctlsf_state_t *)state;
  if (scl_unlikely(st == NULL || ptr == NULL))
    return;

  SCL_SCOPE_LOCK(&st->pool_lock) {
    ctlsf_hdr_t *block = (ctlsf_hdr_t *)((unsigned char *)ptr - CTLSF_HDR_SZ);
    uint32_t block_size = block->size_flags & CTLSF_SIZE_MASK;

    /* Wipe user data before coalescing (security hardening). */
    scl_secure_zero((unsigned char *)block + CTLSF_HDR_SZ,
                    (size_t)block_size - CTLSF_HDR_SZ);

    /* ── Coalesce with previous physical block ─────────── */
    if (scl_unlikely(block->size_flags & CTLSF_PREV_FREE)) {
      ctlsf_hdr_t *prev = block->prev_phys;
      if (scl_likely(prev != NULL)) {
        uint32_t prev_size = prev->size_flags & CTLSF_SIZE_MASK;
        int pfl, psl;
        ctlsf_mapping_insert(prev_size, &pfl, &psl);
        ctlsf_bin_pop(st, pfl, psl, prev);
        prev->size_flags = (prev_size + block_size) |
                           (prev->size_flags & CTLSF_PREV_FREE) |
                           CTLSF_FREE_FLAG;
        block = prev;
        block_size = prev_size + block_size;
      }
    }

    /* ── Coalesce with next physical block ─────────────── */
    ctlsf_hdr_t *next = (ctlsf_hdr_t *)((unsigned char *)block + block_size);
    if (scl_likely((unsigned char *)next < st->pool_end) &&
        (next->size_flags & CTLSF_FREE_FLAG)) {
      uint32_t next_size = next->size_flags & CTLSF_SIZE_MASK;
      int nfl, nsl;
      ctlsf_mapping_insert(next_size, &nfl, &nsl);
      ctlsf_bin_pop(st, nfl, nsl, next);
      block_size += next_size;
    }

    /* Finalise the merged block. */
    block->size_flags =
        block_size | (block->size_flags & CTLSF_PREV_FREE) | CTLSF_FREE_FLAG;

    /* Tell the block after the merged region that its predecessor
     * is now free, and fix up its prev_phys link. */
    ctlsf_hdr_t *after = (ctlsf_hdr_t *)((unsigned char *)block + block_size);
    if (scl_likely((unsigned char *)after < st->pool_end)) {
      after->prev_phys = block;
      after->size_flags |= CTLSF_PREV_FREE;
    }

    /* Insert the merged block into its bin. */
    int mfl, msl;
    ctlsf_mapping_insert(block_size, &mfl, &msl);
    ctlsf_bin_push(st, mfl, msl, block);
  }
}

/* ── create / destroy ──────────────────────────────────────── */
scl_allocator_t *scl_calloc_tlsf_create(scl_allocator_t *backing,
                                         size_t memory_size,
                                         size_t alignment) {
   if (scl_unlikely(backing == NULL || memory_size < 4096))
     return NULL;
   if (alignment == 0)
     alignment = alignof(max_align_t);
   if (scl_unlikely(!scl_is_pow2_sz(alignment)))
     return NULL;

   /* Allocate the state descriptor from the backing allocator. */
   ctlsf_state_t *st = (ctlsf_state_t *)scl_alloc(backing, sizeof(ctlsf_state_t),
                                                  alignof(max_align_t));
   if (scl_unlikely(st == NULL))
     return NULL;

   scl_memset(st, 0, sizeof(*st));
   st->backing = backing;

   /* Align the pool size up to `alignment`. */
   size_t actual = memory_size;
   size_t mod = actual % alignment;
   if (mod != 0)
     actual += alignment - mod;

   /* The pool must hold at least one minimal block. */
   if (scl_unlikely(actual < CTLSF_HDR_SZ + CTLSF_MIN_BLOCK)) {
     scl_free(backing, st);
     return NULL;
   }

   st->pool = scl_alloc(backing, actual, alignment);
  if (scl_unlikely(st->pool == NULL)) {
    scl_free(backing, st);
    return NULL;
  }
  st->pool_size = actual;
  st->pool_end = (unsigned char *)st->pool + actual;

  /* Initialise the global pool lock. */
  scl_spinlock_init(&st->pool_lock);

  /* Initialise every per-bin spinlock. */
  for (int f = 0; f < CTLSF_FL_MAX; f++) {
    atomic_init(&st->sl_bm[f], 0u);
    for (int s = 0; s < CTLSF_SL_COUNT; s++) {
      scl_spinlock_init(&st->bins[f][s].lock);
      st->bins[f][s].head = NULL;
    }
  }
  atomic_init(&st->fl_bm, 0u);

  /* Seed the pool with one large free block spanning the entire
   * region.  Its size includes the full header; user payload begins
   * at offset CTLSF_HDR_SZ. */
  ctlsf_hdr_t *first = (ctlsf_hdr_t *)st->pool;
  first->prev_phys = NULL;
  first->next_free = NULL;
  first->prev_free = NULL;
  first->size_flags = (uint32_t)actual | CTLSF_FREE_FLAG;

  int ffl, fsl;
  ctlsf_mapping_insert(actual, &ffl, &fsl);
  ctlsf_bin_push(st, ffl, fsl, first);

  /* Allocate the public allocator handle. */
  scl_allocator_t *alloc = (scl_allocator_t *)scl_alloc(
      backing, sizeof(scl_allocator_t), alignof(max_align_t));
  if (scl_unlikely(alloc == NULL)) {
    scl_free(backing, st->pool);
    scl_free(backing, st);
    return NULL;
  }

  alloc->malloc_fn = ctlsf_malloc_fn;
  alloc->calloc_fn = ctlsf_calloc_fn;
  alloc->realloc_fn = ctlsf_realloc_fn;
  alloc->free_fn = ctlsf_free_fn;
  alloc->state = st;
  return alloc;
}

void scl_calloc_tlsf_destroy(scl_allocator_t *alloc) {
  if (scl_unlikely(alloc == NULL))
    return;
  ctlsf_state_t *st = (ctlsf_state_t *)alloc->state;
  scl_allocator_t *backing = st->backing;

  /* Securely wipe the pool before returning it to the backing store. */
  if (st->pool != NULL)
    scl_secure_zero(st->pool, st->pool_size);

  scl_free(backing, st->pool);
  scl_free(backing, st);
  scl_free(backing, alloc);
}
