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

/* Concurrent fixed-size object pool allocator.
 *
 * Thread-safe lock-free variant of scl_alloc_pool. Uses intrusive Treiber-stack
 * free-list with atomic DWCAS (double-width CAS) operations for ABA prevention;
 * no spinlocks or kernel transitions. Returned blocks are securely zeroed before
 * being placed back on the freelist so that no caller ever observes another
 * caller's stale payload (runtime security).
 *
 * ABA defense: the free_list is a tagged pointer (scl_dwcas_t) combining a
 * 64-bit pointer with a 64-bit tag/counter. Every push increments the tag,
 * so a stale pointer value from an earlier epoch will fail the DWCAS. */

#include "scl_concurrent_alloc_pool.h"
#include "scl_concurrent_common.h"
#include "scl_stdalign.h"
#include "scl_string.h"
#include <stdint.h>

typedef struct {
  scl_allocator_t *backing;
  void *chunk;
  scl_dwcas_t free_list; /* DWCAS-based intrusive stack (tagged ptr for ABA) */
  /* 1 bit per block; 1 = free, 0 = allocated. Atomic because bits of
   * eight neighbouring blocks share a byte: a plain read-modify-write
   * races with frees/allocs of adjacent blocks and loses their update. */
  _Atomic(uint8_t) *free_bitmap;
  size_t block_size;
  size_t total_blocks;
  _Atomic(size_t)
      free_count; /* approximate; only read, never modified by caller code */
} calloc_pool_state_t;

/* ── vtable helpers ────────────────────────────────────────── */
static void *calloc_pool_malloc_fn(void *state, size_t size, size_t alignment);
static void *calloc_pool_calloc_fn(void *state, size_t count, size_t size,
                                    size_t alignment);
static void *calloc_pool_realloc_fn(void *state, void *ptr, size_t old_size,
                                     size_t new_size, size_t alignment);
static void calloc_pool_free_fn(void *state, void *ptr);

/* ── alloc ─────────────────────────────────────────────────── */
static void *calloc_pool_malloc_fn(void *state, size_t size, size_t alignment) {
  (void)alignment;
  calloc_pool_state_t *p = (calloc_pool_state_t *)state;
  if (scl_unlikely(!p || size == 0 || size > p->block_size))
    return NULL;

  /* DWCAS-based lock-free pop from intrusive tagged stack.
   * Load-acquire ensures we see any prior modifications by the thread
   * that freed this block. */
  void *block;
  scl_dwcas_t expected, desired;
for (;;) {
     expected = scl_dwcas_load(&p->free_list);
     block = (void *)expected.lo;
     if (scl_unlikely(block == NULL))
       return NULL;

     /* Next pointer becomes the new head. Atomic relaxed load: a free
      * of this block racing in the ABA window may rewrite the link —
      * the tag makes our CAS fail then, but the read itself must not
      * be a plain (UB) data race. */
     desired.lo =
         atomic_load_explicit((_Atomic uintptr_t *)block, memory_order_relaxed);
     desired.hi = expected.hi + 1; /* bump tag to defeat ABA */

     if (scl_likely(scl_dwcas(&p->free_list, &expected, desired))) {
       /* Clear the block's free bit (1 = free, so a block just popped
        * off the freelist must have its bit set). */
       size_t idx =
           ((unsigned char *)block - (unsigned char *)p->chunk) / p->block_size;
       uint8_t mask = 1U << (idx % 8);
       uint8_t old_val = atomic_fetch_and_explicit(
           &p->free_bitmap[idx / 8], (uint8_t)~mask, memory_order_acq_rel);
       if (scl_unlikely(!(old_val & mask))) {
         /* Block was on the freelist while marked allocated — freelist
          * corruption. Fail the allocation and drop the block from
          * circulation rather than hand out possibly-live memory. */
         return NULL;
       }

      /* Decrement approximate free count (not strictly consistent). */
      atomic_fetch_sub_explicit(&p->free_count, 1, memory_order_relaxed);
      return block;
    }
    /* CAS failed — another thread modified free_list; retry. */
  }
}

static void *calloc_pool_calloc_fn(void *state, size_t count, size_t size,
                                    size_t alignment) {
  size_t total;
  if (scl_unlikely(scl_mul_overflow(count, size, &total)))
    return NULL;
  void *ptr = calloc_pool_malloc_fn(state, total, alignment);
  if (scl_likely(ptr))
    scl_memset(ptr, 0, total);
  return ptr;
}

static void *calloc_pool_realloc_fn(void *state, void *ptr, size_t old_size,
                                     size_t new_size, size_t alignment) {
  if (scl_unlikely(!ptr))
    return calloc_pool_malloc_fn(state, new_size, alignment);
  if (scl_unlikely(new_size == 0)) {
    calloc_pool_free_fn(state, ptr);
    return NULL;
  }

  void *new_ptr = calloc_pool_malloc_fn(state, new_size, alignment);
  if (scl_likely(new_ptr)) {
    size_t copy = old_size < new_size ? old_size : new_size;
    scl_memcpy(new_ptr, ptr, copy);
    calloc_pool_free_fn(state, ptr);
  }
  return new_ptr;
}

/* ── free ──────────────────────────────────────────────────── */
static void calloc_pool_free_fn(void *state, void *ptr) {
  calloc_pool_state_t *p = (calloc_pool_state_t *)state;
  if (scl_unlikely(!p || !ptr))
    return;

  /* Bounds + alignment check (no lock needed; chunk/block_size are
   * immutable after create). */
  unsigned char *start = (unsigned char *)p->chunk;
  unsigned char *end = start + p->block_size * p->total_blocks;
  if ((unsigned char *)ptr < start || (unsigned char *)ptr >= end)
    return;

  size_t offset = (unsigned char *)ptr - start;
  if (offset % p->block_size != 0)
    return;

  /* Double-free detection via bitmap. Atomic fetch_or: of two racing
   * frees of the same block, exactly one observes the bit clear and
   * proceeds; the other returns here. */
  size_t idx = offset / p->block_size;
  uint8_t mask = 1U << (idx % 8);
  uint8_t old_val = atomic_fetch_or_explicit(&p->free_bitmap[idx / 8], mask,
                                             memory_order_acq_rel);
  if (old_val & mask)
    return; /* already free — double-free rejected */

  /* Runtime security: wipe the payload before exposing the block to any
   * future caller. This is done before the lock-free push so the block
   * is already zeroed when any other thread acquires it. The first word
   * is skipped: it is the intrusive link, only ever touched via atomic
   * ops (a stale popper may read it concurrently), and it is overwritten
   * by the atomic link store below anyway. */
  scl_secure_zero((unsigned char *)ptr + sizeof(void *),
                  p->block_size - sizeof(void *));

  /* DWCAS-based lock-free push onto intrusive tagged stack.
   * We store the next pointer relaxed-ordered (the block was just zeroed,
   * so the link field is 0), then atomically swap the head with a tag bump.
   * Store-release ensures the zeroed block is visible to threads that pop it.
   * The tag increment prevents ABA: a stale pointer from an earlier epoch
   * will have a mismatched tag and fail the CAS. */
scl_dwcas_t expected, desired;
   for (;;) {
     expected = scl_dwcas_load(&p->free_list);
     /* Store current head as this block's next link. Atomic relaxed:
      * pairs with the popper's relaxed load; publication ordering is
      * provided by the DWCAS itself. */
     atomic_store_explicit((_Atomic uintptr_t *)ptr, expected.lo,
                           memory_order_relaxed);
     desired.lo = (uintptr_t)ptr;
     desired.hi = expected.hi + 1; /* bump tag on every push */
     if (scl_likely(scl_dwcas(&p->free_list, &expected, desired))) {
      /* Increment approximate free count (may lag slightly behind reality). */
      atomic_fetch_add_explicit(&p->free_count, 1, memory_order_relaxed);
      return;
    }
    /* CAS failed — another thread modified free_list; retry. */
  }
}

/* ── create / destroy ──────────────────────────────────────── */
scl_allocator_t *scl_calloc_pool_create(scl_allocator_t *backing,
                                         size_t block_size, size_t block_count,
                                         size_t alignment) {
if (scl_unlikely(!backing || block_size == 0 || block_count == 0))
     return NULL;

   if (alignment == 0)
     alignment = alignof(max_align_t);
   if (scl_unlikely(!scl_is_pow2_sz(alignment)))
     return NULL;

   /* Round block_size up to alignment so every block is properly aligned. */
   block_size = scl_align_forward(block_size, alignment);
   if (block_size < sizeof(void *))
     block_size = sizeof(void *);

   calloc_pool_state_t *state = (calloc_pool_state_t *)scl_alloc(
       backing, sizeof(calloc_pool_state_t), alignof(max_align_t));
   if (scl_unlikely(!state))
     return NULL;

   state->backing = backing;
   state->block_size = block_size;
   state->total_blocks = block_count;
   atomic_init(&state->free_count, 0);
   state->free_list.raw = 0; /* null ptr, tag = 0 */
   state->chunk = NULL;
   state->free_bitmap = NULL;

   size_t total;
   if (scl_unlikely(scl_mul_overflow(block_size, block_count, &total))) {
     scl_free(backing, state);
     return NULL;
   }

   state->chunk = scl_alloc(backing, total, alignment);
  if (scl_unlikely(!state->chunk)) {
    scl_free(backing, state);
    return NULL;
  }

  /* Allocate free-bitmap: one bit per block, rounded up to byte boundary. */
  size_t bitmap_bytes = (block_count + 7) / 8;
  state->free_bitmap = (_Atomic(uint8_t) *)scl_alloc(backing, bitmap_bytes,
                                                     alignof(max_align_t));
  if (scl_unlikely(!state->free_bitmap)) {
    scl_free(backing, state->chunk);
    scl_free(backing, state);
    return NULL;
  }
  /* All blocks start free → all bits set to 1. (Plain memset is fine:
   * create runs single-threaded, before the pool is published.) */
  memset((void *)state->free_bitmap, 0xFF, bitmap_bytes);

  /* Build the freelist: each block's first sizeof(void*) bytes store the
   * next pointer. Walked back-to-front so free_list points at block 0. */
  unsigned char *base = (unsigned char *)state->chunk;
  void *prev = NULL;
  for (size_t i = 0; i < block_count; i++) {
    void *block = base + i * block_size;
    *(void **)block = prev;
    prev = block;
  }
scl_dwcas_t init_head;
   init_head.lo = (uintptr_t)prev;
   init_head.hi = 0;
  scl_dwcas_store(&state->free_list, init_head);
  atomic_store_explicit(&state->free_count, block_count, memory_order_release);

  scl_allocator_t *alloc = (scl_allocator_t *)scl_alloc(
      backing, sizeof(scl_allocator_t), alignof(max_align_t));
  if (scl_unlikely(!alloc)) {
    scl_free(backing, state->free_bitmap);
    scl_free(backing, state->chunk);
    scl_free(backing, state);
    return NULL;
  }

  alloc->malloc_fn = calloc_pool_malloc_fn;
  alloc->calloc_fn = calloc_pool_calloc_fn;
  alloc->realloc_fn = calloc_pool_realloc_fn;
  alloc->free_fn = calloc_pool_free_fn;
  alloc->state = state;
  return alloc;
}

void scl_calloc_pool_destroy(scl_allocator_t *alloc) {
  if (scl_unlikely(!alloc))
    return;
  calloc_pool_state_t *p = (calloc_pool_state_t *)alloc->state;
  if (scl_unlikely(!p))
    return;

  scl_allocator_t *backing = p->backing;

  /* Wipe the entire chunk before returning it to the backing allocator. */
  if (p->chunk) {
    size_t total = p->block_size * p->total_blocks;
    scl_secure_zero(p->chunk, total);
    scl_free(backing, p->chunk);
  }
  if (p->free_bitmap)
    scl_free(backing, p->free_bitmap);
  scl_secure_zero(p, sizeof(*p));
  scl_free(backing, p);
  scl_free(backing, alloc);
}