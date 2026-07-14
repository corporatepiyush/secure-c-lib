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

/* Concurrent slab allocator. Per-bucket spinlocks guard singly-linked
 * free-lists; all slab chunks are pre-allocated at init so the hot path
 * performs no system allocation. 48 size buckets (8B .. 4MB).
 *
 * NOTE on SCL_SCOPE_LOCK + break: the macro expands to a for-loop, so a
 * `break` inside it only exits that inner for-loop. Outer control flow
 * (e.g. the bucket-scan loop in free_fn) must use explicit flags rather
 * than relying on break to escape the locked region. */

#include "scl_concurrent_alloc_slab.h"
#include "scl_concurrent_common.h"
#include "scl_stdalign.h"
#include "scl_string.h"

#define SCL_CSLAB_DEFAULT_NUM 48

/* 48 buckets, 8B .. 4MB. Powers of two plus intermediate sizes for finer
 * granularity in the small-object range where it matters most. */
static const size_t scl_cslab_default_sizes[SCL_CSLAB_DEFAULT_NUM] = {
    8,      10,     13,     16,      20,      24,      32,      40,
    48,     64,     80,     96,      128,     160,     192,     256,
    320,    384,    512,    640,     768,     1024,    1536,    1792,
    2048,   3072,   4096,   5120,    6144,    8192,    12288,   16384,
    24576,  32768,  49152,  65536,   98304,   131072,  196608,  262144,
    393216, 524288, 786432, 1048576, 1572864, 2097152, 3145728, 4194304};

typedef struct {
  void *chunk;         /* contiguous backing buffer            */
  void *free_list;     /* singly-linked list of free blocks   */
  scl_spinlock_t lock; /* guards free_list + free_count       */
  size_t block_size;   /* usable bytes per block (>= sizeof(void*)) */
  size_t total_blocks; /* blocks carved out of chunk          */
  size_t free_count;   /* currently available blocks          */
  uint8_t
      _pad[80]; /* pad to 128 bytes (one cache line on ARM64/modern x86-64) */
} cslab_pool_t;

typedef struct {
  scl_allocator_t *backing;
  size_t num_buckets;
  size_t *bucket_sizes;
  cslab_pool_t *pools;
} cslab_state_t;

/* Block count per bucket — denser pools for small objects (better cache
 * residency and amortised init cost), sparser for large ones (cap memory). */
static SCL_ALWAYS_INLINE SCL_PURE size_t cslab_block_count(size_t block_size) {
  if (block_size <= 64)
    return 2048;
  if (block_size <= 256)
    return 1024;
  if (block_size <= 1024)
    return 512;
  if (block_size <= 4096)
    return 256;
  if (block_size <= 16384)
    return 128;
  return 64;
}

/* Find the smallest bucket whose block_size >= `size`. Returns num_buckets
 * if no bucket fits (size exceeds the largest bucket). */
static SCL_ALWAYS_INLINE SCL_PURE size_t
cslab_pool_index(const cslab_state_t *s, size_t size) {
  /* Binary search over sorted bucket sizes for O(log N) lookup. */
  size_t lo = 0, hi = s->num_buckets;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (size <= s->bucket_sizes[mid])
      hi = mid;
    else
      lo = mid + 1;
  }
  return lo; /* == s->num_buckets if no bucket fits */
}

static void cslab_free_fn(void *state, void *ptr);

/* Carve `blocks` free-list nodes out of one contiguous chunk. Each block
 * is `aligned` bytes; the first sizeof(void*) bytes of a free block hold
 * the next pointer. Returns 1 on success, 0 on allocation failure. */
static int cslab_pool_init(cslab_state_t *s, cslab_pool_t *p,
                            size_t block_size, size_t alignment) {
   size_t blocks = cslab_block_count(block_size);
   size_t aligned = block_size;
   if (aligned < sizeof(void *))
     aligned = sizeof(void *);
   if (alignment > alignof(max_align_t))
     aligned = scl_align_forward(aligned, alignment);

   size_t total;
   if (scl_unlikely(scl_mul_overflow(aligned, blocks, &total)))
     return 0;

   p->chunk =
       s->backing->malloc_fn(s->backing->state, total, alignment);
  if (scl_unlikely(!p->chunk))
    return 0;

  unsigned char *base = (unsigned char *)p->chunk;
  void *prev = NULL;
  for (size_t i = 0; i < blocks; i++) {
    void *block = base + i * aligned;
    *(void **)block = prev;
    prev = block;
  }

  p->free_list = prev;
  p->block_size = aligned;
  p->total_blocks = blocks;
  p->free_count = blocks;
  scl_spinlock_init(&p->lock);
  return 1;
}

static void *cslab_malloc_fn(void *state, size_t size, size_t alignment) {
  (void)alignment;
  cslab_state_t *s = (cslab_state_t *)state;
  if (scl_unlikely(!s || size == 0))
    return NULL;

  size_t idx = cslab_pool_index(s, size);
  if (scl_unlikely(idx >= s->num_buckets))
    return NULL;

  cslab_pool_t *p = &s->pools[idx];
  void *block = NULL;
  SCL_SCOPE_LOCK(&p->lock) {
    if (scl_likely(p->free_list != NULL)) {
      block = p->free_list;
      p->free_list = *(void **)block;
      p->free_count--;
    }
  }
  return block;
}

static void *cslab_calloc_fn(void *state, size_t count, size_t size,
                             size_t alignment) {
  size_t total;
  if (scl_unlikely(scl_mul_overflow(count, size, &total)))
    return NULL;
  void *ptr = cslab_malloc_fn(state, total, alignment);
  if (scl_likely(ptr))
    scl_memset(ptr, 0, total);
  return ptr;
}

static void *cslab_realloc_fn(void *state, void *ptr, size_t old_size,
                              size_t new_size, size_t alignment) {
  if (scl_unlikely(!ptr))
    return cslab_malloc_fn(state, new_size, alignment);
  if (scl_unlikely(new_size == 0)) {
    cslab_free_fn(state, ptr);
    return NULL;
  }

  cslab_state_t *s = (cslab_state_t *)state;
  if (scl_unlikely(!s))
    return NULL;

  size_t old_idx = cslab_pool_index(s, old_size);
  size_t new_idx = cslab_pool_index(s, new_size);

  /* Same bucket → the existing block already has room for the new size. */
  if (old_idx == new_idx && old_idx < s->num_buckets)
    return ptr;

  /* Different bucket → allocate fresh, copy, free old. */
  void *new_ptr = cslab_malloc_fn(state, new_size, alignment);
  if (scl_likely(new_ptr)) {
    size_t copy = old_size < new_size ? old_size : new_size;
    scl_memcpy(new_ptr, ptr, copy);
    cslab_free_fn(state, ptr);
  }
  return new_ptr;
}

/* Locate the owning bucket by range-checking the pointer against each
 * chunk's [chunk, chunk + block_size*total_blocks) extent, then push the
 * block back onto that bucket's free-list under its lock.
 *
 * IMPORTANT: a `break` inside SCL_SCOPE_LOCK only exits the lock scope's
 * inner for-loop, not the bucket-scan loop. We use a `found` flag to
 * short-circuit the outer scan once the block has been returned. */
static void cslab_free_fn(void *state, void *ptr) {
  cslab_state_t *s = (cslab_state_t *)state;
  if (scl_unlikely(!s || !ptr))
    return;

  int found = 0;
  for (size_t i = 0; i < s->num_buckets && !found; i++) {
    cslab_pool_t *p = &s->pools[i];
    unsigned char *start = (unsigned char *)p->chunk;
    unsigned char *end = start + p->block_size * p->total_blocks;
    if ((unsigned char *)ptr < start || (unsigned char *)ptr >= end)
      continue;

    /* ptr belongs to this bucket. Wipe then return to free-list. */
    scl_secure_zero(ptr, p->block_size);
    SCL_SCOPE_LOCK(&p->lock) {
      if (p->free_count < p->total_blocks) {
        *(void **)ptr = p->free_list;
        p->free_list = ptr;
        p->free_count++;
      }
      /* If the pool is already full (shouldn't happen for a balanced
       * alloc/free discipline), drop the block — its memory remains
       * owned by the chunk and is reclaimed at destroy time. */
    }
    found = 1;
  }
}

scl_allocator_t *scl_calloc_slab_create(scl_allocator_t *backing,
                                         const size_t *bucket_sizes,
                                         size_t num_buckets,
                                         size_t alignment) {
   if (scl_unlikely(!backing))
     return NULL;
   if (alignment == 0)
     alignment = alignof(max_align_t);
   if (scl_unlikely(!scl_is_pow2_sz(alignment)))
     return NULL;

   if (scl_unlikely(!bucket_sizes || num_buckets == 0)) {
     bucket_sizes = scl_cslab_default_sizes;
     num_buckets = SCL_CSLAB_DEFAULT_NUM;
   }

  cslab_state_t *state = (cslab_state_t *)backing->malloc_fn(
      backing->state, sizeof(cslab_state_t), alignof(max_align_t));
  if (scl_unlikely(!state))
    return NULL;

  state->backing = backing;
  state->num_buckets = num_buckets;

  state->bucket_sizes = (size_t *)backing->malloc_fn(
      backing->state, num_buckets * sizeof(size_t), alignof(max_align_t));
  if (scl_unlikely(!state->bucket_sizes)) {
    backing->free_fn(backing->state, state);
    return NULL;
  }
  for (size_t i = 0; i < num_buckets; i++)
    state->bucket_sizes[i] = bucket_sizes[i];

  state->pools = (cslab_pool_t *)backing->malloc_fn(
      backing->state, num_buckets * sizeof(cslab_pool_t), alignof(max_align_t));
  if (scl_unlikely(!state->pools)) {
    backing->free_fn(backing->state, state->bucket_sizes);
    backing->free_fn(backing->state, state);
    return NULL;
  }
  for (size_t i = 0; i < num_buckets; i++)
    scl_memset(&state->pools[i], 0, sizeof(cslab_pool_t));

  for (size_t i = 0; i < num_buckets; i++) {
if (scl_unlikely(!cslab_pool_init(state, &state->pools[i],
                                       state->bucket_sizes[i], alignment))) {
      for (size_t j = 0; j < i; j++)
        backing->free_fn(backing->state, state->pools[j].chunk);
      backing->free_fn(backing->state, state->pools);
      backing->free_fn(backing->state, state->bucket_sizes);
      backing->free_fn(backing->state, state);
      return NULL;
    }
  }

  scl_allocator_t *alloc = (scl_allocator_t *)backing->malloc_fn(
      backing->state, sizeof(scl_allocator_t), alignof(max_align_t));
  if (scl_unlikely(!alloc)) {
    for (size_t i = 0; i < num_buckets; i++)
      backing->free_fn(backing->state, state->pools[i].chunk);
    backing->free_fn(backing->state, state->pools);
    backing->free_fn(backing->state, state->bucket_sizes);
    backing->free_fn(backing->state, state);
    return NULL;
  }

  alloc->malloc_fn = cslab_malloc_fn;
  alloc->calloc_fn = cslab_calloc_fn;
  alloc->realloc_fn = cslab_realloc_fn;
  alloc->free_fn = cslab_free_fn;
  alloc->state = state;
  return alloc;
}

void scl_calloc_slab_destroy(scl_allocator_t *alloc) {
  if (scl_unlikely(!alloc))
    return;
  cslab_state_t *s = (cslab_state_t *)alloc->state;
  scl_allocator_t *backing = s->backing;

  for (size_t i = 0; i < s->num_buckets; i++) {
    cslab_pool_t *p = &s->pools[i];
    if (p->chunk) {
      /* Wipe the entire slab chunk before returning it to the backing
       * allocator so no freed object contents leak through. */
      scl_secure_zero(p->chunk, p->block_size * p->total_blocks);
      backing->free_fn(backing->state, p->chunk);
    }
  }
  backing->free_fn(backing->state, s->pools);
  backing->free_fn(backing->state, s->bucket_sizes);
  scl_secure_zero(s, sizeof(cslab_state_t));
  backing->free_fn(backing->state, s);
  scl_secure_zero(alloc, sizeof(scl_allocator_t));
  backing->free_fn(backing->state, alloc);
}
