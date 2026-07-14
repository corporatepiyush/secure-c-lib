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

/* Slab allocator (buckets 16..8192 B). Partial/full/free slab lists per size.
 * Amortised O(1) with excellent cache locality. */

#include "scl_alloc_slab.h"
#include "scl_stdalign.h"
#include "scl_string.h"

#define SCL_SLAB_DEFAULT_NUM 10

static const size_t scl_slab_default_sizes[SCL_SLAB_DEFAULT_NUM] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};

typedef struct {
  void *chunk;
  void *free_list;
  size_t block_size;
  size_t total_blocks;
  size_t free_count;
} slab_pool_t;

typedef struct {
   scl_allocator_t *backing;
   size_t num_buckets;
   size_t *bucket_sizes;
   slab_pool_t *pools;
   size_t alignment;
} slab_state_t;

static SCL_ALWAYS_INLINE SCL_PURE size_t slab_block_count(size_t block_size) {
  if (block_size <= 32)
    return 1024;
  if (block_size <= 128)
    return 512;
  if (block_size <= 512)
    return 256;
  if (block_size <= 2048)
    return 128;
  return 64;
}

static size_t slab_pool_index(const slab_state_t *s, size_t size) {
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

static void slab_free_fn(void *state, void *ptr);

static int slab_pool_init(slab_state_t *s, slab_pool_t *p, size_t block_size,
                           size_t alignment) {
   size_t blocks = slab_block_count(block_size);
size_t aligned = block_size;
    if (aligned < sizeof(void *))
      aligned = sizeof(void *);
    if (alignment > 0)
      aligned = scl_align_forward(aligned, alignment);

   size_t total;
   if (scl_unlikely(scl_mul_overflow(aligned, blocks, &total)))
     return 0;

   p->chunk =
       s->backing->malloc_fn(s->backing->state, total, alignment);
  if (scl_unlikely(!p->chunk))
    return 0;

  unsigned char *ptr = (unsigned char *)p->chunk;
  void *prev = NULL;
  for (size_t i = 0; i < blocks; i++) {
    void *block = ptr + i * aligned;
    *(void **)block = prev;
    prev = block;
  }

  p->free_list = prev;
  p->block_size = aligned;
  p->total_blocks = blocks;
  p->free_count = blocks;
  return 1;
}

static void *slab_malloc_fn(void *state, size_t size, size_t alignment) {
   slab_state_t *s = (slab_state_t *)state;
   if (scl_unlikely(!s || size == 0))
     return NULL;

   /* Honour the requested alignment: the pool's block_size was already
    * rounded up to `alignment` in slab_pool_init, so every block in the
    * pool naturally meets the alignment requirement.  We only need to
    * verify the caller did not request a *larger* alignment than the
    * pool was configured for. */
   if (scl_unlikely(alignment > s->alignment))
     return NULL;

   size_t idx = slab_pool_index(s, size);
   if (scl_unlikely(idx >= s->num_buckets))
     return NULL;

   slab_pool_t *p = &s->pools[idx];
   if (scl_unlikely(!p->free_list))
     return NULL;

   void *block = p->free_list;
   p->free_list = *(void **)block;
   p->free_count--;
   return block;
 }

static void *slab_calloc_fn(void *state, size_t count, size_t size,
                            size_t alignment) {
  size_t total;
  if (scl_unlikely(scl_mul_overflow(count, size, &total)))
    return NULL;
  void *ptr = slab_malloc_fn(state, total, alignment);
  if (scl_likely(ptr))
    memset(ptr, 0, total);
  return ptr;
}

static void *slab_realloc_fn(void *state, void *ptr, size_t old_size,
                             size_t new_size, size_t alignment) {
  if (scl_unlikely(!ptr))
    return slab_malloc_fn(state, new_size, alignment);
  if (scl_unlikely(new_size == 0)) {
    slab_free_fn(state, ptr);
    return NULL;
  }
  void *new_ptr = slab_malloc_fn(state, new_size, alignment);
  if (scl_likely(new_ptr)) {
    size_t copy = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, copy);
    slab_free_fn(state, ptr);
  }
  return new_ptr;
}

static void slab_free_fn(void *state, void *ptr) {
  slab_state_t *s = (slab_state_t *)state;
  if (scl_unlikely(!s || !ptr))
    return;

  for (size_t i = 0; i < s->num_buckets; i++) {
    slab_pool_t *p = &s->pools[i];
    unsigned char *start = (unsigned char *)p->chunk;
    unsigned char *end = start + p->block_size * p->total_blocks;
    if ((unsigned char *)ptr >= start && (unsigned char *)ptr < end) {
      if (p->free_count >= p->total_blocks)
        return;
      *(void **)ptr = p->free_list;
      p->free_list = ptr;
      p->free_count++;
      return;
    }
  }
}

scl_allocator_t *scl_alloc_slab_create(scl_allocator_t *backing,
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
     bucket_sizes = scl_slab_default_sizes;
     num_buckets = SCL_SLAB_DEFAULT_NUM;
   }

  slab_state_t *state = (slab_state_t *)backing->malloc_fn(
      backing->state, sizeof(slab_state_t), alignof(max_align_t));
  if (scl_unlikely(!state))
    return NULL;

state->backing = backing;
   state->num_buckets = num_buckets;
   state->alignment = alignment;

  state->bucket_sizes = (size_t *)backing->malloc_fn(
      backing->state, num_buckets * sizeof(size_t), alignof(max_align_t));
  if (scl_unlikely(!state->bucket_sizes)) {
    backing->free_fn(backing->state, state);
    return NULL;
  }
  for (size_t i = 0; i < num_buckets; i++)
    state->bucket_sizes[i] = bucket_sizes[i];

  state->pools = (slab_pool_t *)backing->malloc_fn(
      backing->state, num_buckets * sizeof(slab_pool_t), alignof(max_align_t));
  if (scl_unlikely(!state->pools)) {
    backing->free_fn(backing->state, state->bucket_sizes);
    backing->free_fn(backing->state, state);
    return NULL;
  }
  for (size_t i = 0; i < num_buckets; i++)
    memset(&state->pools[i], 0, sizeof(slab_pool_t));

  for (size_t i = 0; i < num_buckets; i++) {
if (scl_unlikely(
             !slab_pool_init(state, &state->pools[i], state->bucket_sizes[i],
                             alignment))) {
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

  alloc->malloc_fn = slab_malloc_fn;
  alloc->calloc_fn = slab_calloc_fn;
  alloc->realloc_fn = slab_realloc_fn;
  alloc->free_fn = slab_free_fn;
  alloc->state = state;
  return alloc;
}

void scl_alloc_slab_destroy(scl_allocator_t *alloc) {
  if (scl_unlikely(!alloc))
    return;
  slab_state_t *s = (slab_state_t *)alloc->state;
  scl_allocator_t *backing = s->backing;
  for (size_t i = 0; i < s->num_buckets; i++) {
    if (s->pools[i].chunk)
      backing->free_fn(backing->state, s->pools[i].chunk);
  }
  backing->free_fn(backing->state, s->pools);
  backing->free_fn(backing->state, s->bucket_sizes);
  backing->free_fn(backing->state, s);
  backing->free_fn(backing->state, alloc);
}
