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

/* Sharded (segmented) dynamic array. Fixed-size shards, stable indices,
 * linear growth (one shard at a time), caller-chosen shard length for cache. */

#include "scl_sharded_array.h"
#include "scl_string.h"

/* The shard directory (array of shard pointers) also grows LINEARLY, by this
 * many slots at a time, so the whole structure expands at a steady rate rather
 * than doubling. */
#define SCL_SHARDED_DIR_GROW 8u

scl_error_t scl_sharded_array_init(scl_allocator_t *alloc,
                                   scl_sharded_array_t *sa, size_t elem_size,
                                   size_t shard_len) {
  if (scl_unlikely(!sa))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(elem_size == 0))
    return SCL_ERR_INVALID_ARG;
  if (shard_len == 0)
    shard_len = SCL_SHARDED_ARRAY_DEFAULT_SHARD;

  /* A shard is shard_len * elem_size bytes; reject a length that would make
   * that product overflow up front so append() never has to. */
  size_t probe;
  if (scl_unlikely(scl_mul_overflow(shard_len, elem_size, &probe)))
    return SCL_ERR_SIZE_OVERFLOW;

  sa->alloc = alloc;
  sa->shards = NULL;
  sa->shard_count = 0;
  sa->dir_cap = 0;
  sa->elem_size = elem_size;
  sa->shard_len = shard_len;
  sa->count = 0;
  return SCL_OK;
}

scl_error_t scl_sharded_array_append(scl_sharded_array_t *sa, const void *elem,
                                     size_t *out_index) {
  if (scl_unlikely(!sa || !elem))
    return SCL_ERR_NULL_PTR;

  size_t shard = sa->count / sa->shard_len;
  size_t off = sa->count % sa->shard_len;

  if (shard >= sa->shard_count) {
    /* Need one more shard. Grow the directory linearly first if needed. */
    if (sa->shard_count >= sa->dir_cap) {
      size_t ndir = sa->dir_cap + SCL_SHARDED_DIR_GROW;
      size_t nbytes;
      if (scl_unlikely(
              scl_mul_overflow(ndir, sizeof(unsigned char *), &nbytes)))
        return SCL_ERR_SIZE_OVERFLOW;
      unsigned char **nd = (unsigned char **)scl_realloc(
          sa->alloc, sa->shards, sa->dir_cap * sizeof(unsigned char *), nbytes,
          alignof(max_align_t));
      if (scl_unlikely(!nd))
        return SCL_ERR_OUT_OF_MEMORY;
      sa->shards = nd;
      sa->dir_cap = ndir;
    }
    size_t sbytes =
        sa->shard_len * sa->elem_size; /* overflow checked in init */
    unsigned char *blk =
        (unsigned char *)scl_alloc(sa->alloc, sbytes, alignof(max_align_t));
    if (scl_unlikely(!blk))
      return SCL_ERR_OUT_OF_MEMORY;
    sa->shards[sa->shard_count++] = blk;
  }

  unsigned char *slot = sa->shards[shard] + off * sa->elem_size;
  scl_memcpy(slot, elem, sa->elem_size);
  if (out_index)
    *out_index = sa->count;
  sa->count++;
  return SCL_OK;
}

void scl_sharded_array_destroy(scl_sharded_array_t *sa) {
  if (!sa)
    return;
  if (sa->shards) {
    for (size_t i = 0; i < sa->shard_count; i++)
      scl_free(sa->alloc, sa->shards[i]);
    scl_free(sa->alloc, sa->shards);
  }
  sa->shards = NULL;
  sa->shard_count = sa->dir_cap = sa->count = 0;
}
