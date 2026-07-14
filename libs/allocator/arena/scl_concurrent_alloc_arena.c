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

/* Concurrent sharded arena (bump) allocator.
 *
 * N independent shards, each with its own spinlock + bump pointer + buffer.
 * A thread hashes pthread_self() to a shard, so distinct threads usually
 * land on distinct shards → low contention.
 *
 * NOTE on cache-line padding: each shard is padded to 128 bytes (one cache line
 * on x86-64 Zen 5+, EPYC, and macOS ARM64/Intel). This eliminates false-sharing
 * MESI invalidations on adjacent shards' data. The entire struct is sized to
 * fill exactly one cache line, so per-shard spinlock contention is fully
 * isolated from other shards. */

#include "scl_concurrent_alloc_arena.h"
#include "scl_concurrent_common.h"
#include "scl_string.h"

#include "scl_pthread.h"
#include "scl_stdalign.h"

#define SCL_CARENA_MAX_SHARDS 64
#define SCL_CARENA_DEFAULT_SHARDS 4

typedef struct {
  scl_spinlock_t lock;
  char *buffer;
  size_t offset;
  size_t capacity;
  size_t bytes_used; /* bytes actually handed out (post-reset) */
  uint8_t
      _pad[88]; /* pad to 128 bytes (one cache line on ARM64/modern x86-64) */
} scl_carena_shard_t;

typedef struct {
  scl_allocator_t *backing;
  size_t num_shards;
  size_t shard_capacity;
  size_t max_capacity;    /* total backing memory cap (0 = unlimited) */
  size_t total_allocated; /* sum of all shard buffer capacities */
  scl_carena_shard_t shards[SCL_CARENA_MAX_SHARDS];
} scl_carena_state_t;

static SCL_ALWAYS_INLINE SCL_PURE size_t scl_carena_align_up(size_t offset,
                                                             size_t align) {
  size_t mask = align - 1;
  return (offset + mask) & ~mask;
}

/* Hash pthread_self() down to a shard index. pthread_t is opaque; we treat
 * its object representation as a bag of bytes and mix with FNV-1a. */
static SCL_ALWAYS_INLINE size_t scl_carena_shard_index(scl_carena_state_t *s) {
  pthread_t tid = pthread_self();
  const unsigned char *p = (const unsigned char *)&tid;
  size_t n = sizeof(tid);
  size_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++) {
    h ^= (size_t)p[i];
    h *= 1099511628211ULL;
  }
  return h & (s->num_shards - 1); /* num_shards is power of two */
}

static bool scl_carena_shard_init(scl_carena_state_t *s, size_t shard_idx,
                                   size_t capacity, size_t alignment) {
   scl_carena_shard_t *sh = &s->shards[shard_idx];
   scl_spinlock_init(&sh->lock);
   sh->buffer = (char *)scl_alloc(s->backing, capacity, alignment);
  if (scl_unlikely(!sh->buffer))
    return false;
  sh->offset = 0;
  sh->capacity = capacity;
  sh->bytes_used = 0;
  return true;
}

static void *scl_carena_malloc_fn(void *state, size_t size, size_t alignment) {
  scl_carena_state_t *s = (scl_carena_state_t *)state;
  if (scl_unlikely(!s || size == 0))
    return NULL;
  if (alignment == 0)
    alignment = alignof(max_align_t);

  size_t idx = scl_carena_shard_index(s);
  scl_carena_shard_t *sh = &s->shards[idx];

  SCL_SCOPE_LOCK(&sh->lock) {
    size_t aligned = scl_carena_align_up(sh->offset, alignment);
    if (scl_likely(aligned <= sh->capacity && size <= sh->capacity - aligned)) {
      void *ptr = sh->buffer + aligned;
      sh->offset = aligned + size;
      sh->bytes_used += size;
      return ptr;
    }

    /* Overflow: grow this shard's buffer (doubling) up to max_capacity. */
    size_t new_cap = sh->capacity * 2;
    size_t need = size + alignment;
    while (new_cap < need) {
      if (scl_unlikely(scl_mul_overflow(new_cap, 2, &new_cap)))
        return NULL;
    }

    if (scl_unlikely(s->max_capacity > 0 &&
                     s->total_allocated + (new_cap - sh->capacity) >
                         s->max_capacity))
      return NULL;

    char *new_buf =
        (char *)scl_alloc(s->backing, new_cap, alignof(max_align_t));
    if (scl_unlikely(!new_buf))
      return NULL;

    /* Preserve in-flight allocations by copying the live prefix. */
    scl_memcpy(new_buf, sh->buffer, sh->offset);

    scl_secure_zero(sh->buffer, sh->capacity);
    scl_free(s->backing, sh->buffer);

    size_t old_cap = sh->capacity;
    sh->buffer = new_buf;
    sh->capacity = new_cap;
    s->total_allocated += (new_cap - old_cap); /* track live backing bytes */

    size_t a0 = scl_carena_align_up(0, alignment);
    void *ptr = sh->buffer + a0;
    sh->offset = a0 + size;
    sh->bytes_used += size;
    return ptr;
  }
  /* Unreachable: SCL_SCOPE_LOCK body always returns from inside. */
  return NULL;
}

static void *scl_carena_calloc_fn(void *state, size_t count, size_t size,
                                  size_t alignment) {
  size_t total;
  if (scl_unlikely(scl_mul_overflow(count, size, &total)))
    return NULL;
  void *ptr = scl_carena_malloc_fn(state, total, alignment);
  if (scl_likely(ptr))
    scl_memset(ptr, 0, total);
  return ptr;
}

static void *scl_carena_realloc_fn(void *state, void *ptr, size_t old_size,
                                   size_t new_size, size_t alignment) {
  if (scl_unlikely(!ptr))
    return scl_carena_malloc_fn(state, new_size, alignment);
  if (scl_unlikely(new_size == 0))
    return NULL;
  void *new_ptr = scl_carena_malloc_fn(state, new_size, alignment);
  if (scl_likely(new_ptr)) {
    size_t copy = old_size < new_size ? old_size : new_size;
    scl_memcpy(new_ptr, ptr, copy);
  }
  return new_ptr;
}

static void scl_carena_free_fn(void *state, void *ptr) {
  (void)state;
  (void)ptr;
  /* Arena contract: individual frees are no-ops. Use reset/destroy. */
}

scl_allocator_t *scl_calloc_arena_create(scl_allocator_t *backing,
                                          size_t capacity, size_t max_capacity,
                                          size_t num_shards,
                                          size_t alignment) {
   if (scl_unlikely(!backing || capacity == 0))
     return NULL;
   if (alignment == 0)
     alignment = alignof(max_align_t);
   if (scl_unlikely(!scl_is_pow2_sz(alignment)))
     return NULL;
   if (num_shards == 0)
    num_shards = SCL_CARENA_DEFAULT_SHARDS;
  if (num_shards > SCL_CARENA_MAX_SHARDS)
    num_shards = SCL_CARENA_MAX_SHARDS;
  /* Power-of-two clamp so shard selection is a bitwise mask. */
  if (!scl_is_pow2_sz(num_shards)) {
    num_shards = scl_bit_ceil_sz(num_shards);
    if (num_shards > SCL_CARENA_MAX_SHARDS)
      num_shards = SCL_CARENA_MAX_SHARDS;
  }

  scl_carena_state_t *state = (scl_carena_state_t *)scl_alloc(
      backing, sizeof(scl_carena_state_t), alignof(max_align_t));
  if (scl_unlikely(!state))
    return NULL;

  state->backing = backing;
  state->num_shards = num_shards;
  state->shard_capacity = capacity;
  state->max_capacity = max_capacity;
  state->total_allocated = 0;

  /* Initialize every slot (even unused ones) so destroy can iterate safely. */
  for (size_t i = 0; i < SCL_CARENA_MAX_SHARDS; i++) {
    state->shards[i].buffer = NULL;
    state->shards[i].offset = 0;
    state->shards[i].capacity = 0;
    state->shards[i].bytes_used = 0;
    scl_spinlock_init(&state->shards[i].lock);
  }

  bool init_ok = true;
  for (size_t i = 0; i < num_shards; i++) {
    if (scl_unlikely(!scl_carena_shard_init(state, i, capacity, alignment))) {
      init_ok = false;
      break;
    }
    state->total_allocated += capacity;
  }

  if (scl_unlikely(!init_ok)) {
    for (size_t i = 0; i < num_shards; i++) {
      if (state->shards[i].buffer) {
        scl_secure_zero(state->shards[i].buffer, state->shards[i].capacity);
        scl_free(backing, state->shards[i].buffer);
      }
    }
    scl_free(backing, state);
    return NULL;
  }

  scl_allocator_t *alloc = (scl_allocator_t *)scl_alloc(
      backing, sizeof(scl_allocator_t), alignof(max_align_t));
  if (scl_unlikely(!alloc)) {
    for (size_t i = 0; i < num_shards; i++) {
      scl_secure_zero(state->shards[i].buffer, state->shards[i].capacity);
      scl_free(backing, state->shards[i].buffer);
    }
    scl_free(backing, state);
    return NULL;
  }

  alloc->malloc_fn = scl_carena_malloc_fn;
  alloc->calloc_fn = scl_carena_calloc_fn;
  alloc->realloc_fn = scl_carena_realloc_fn;
  alloc->free_fn = scl_carena_free_fn;
  alloc->state = state;
  return alloc;
}

void scl_calloc_arena_reset(scl_allocator_t *alloc) {
  if (scl_unlikely(!alloc))
    return;
  scl_carena_state_t *s = (scl_carena_state_t *)alloc->state;

  for (size_t i = 0; i < s->num_shards; i++) {
    scl_carena_shard_t *sh = &s->shards[i];
    SCL_SCOPE_LOCK(&sh->lock) {
#ifndef NDEBUG
      /* Poison the live region in debug builds to catch use-after-reset. */
      scl_memset(sh->buffer, 0xAA, sh->offset);
#endif
      sh->offset = 0;
      sh->bytes_used = 0;
    }
  }
}

void scl_calloc_arena_destroy(scl_allocator_t *alloc) {
  if (scl_unlikely(!alloc))
    return;
  scl_carena_state_t *s = (scl_carena_state_t *)alloc->state;
  scl_allocator_t *backing = s->backing;

  for (size_t i = 0; i < s->num_shards; i++) {
    scl_carena_shard_t *sh = &s->shards[i];
    /* Take the lock to guarantee no in-flight allocator is bumping. */
    SCL_SCOPE_LOCK(&sh->lock) {
      if (sh->buffer) {
        scl_secure_zero(sh->buffer, sh->capacity);
        scl_free(backing, sh->buffer);
        sh->buffer = NULL;
        sh->capacity = 0;
        sh->offset = 0;
        sh->bytes_used = 0;
      }
    }
  }

  scl_secure_zero(s, sizeof(*s));
  scl_free(backing, s);
  scl_secure_zero(alloc, sizeof(*alloc));
  scl_free(backing, alloc);
}
