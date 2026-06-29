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

#ifndef SCL_SHARDED_ARRAY_H
#define SCL_SHARDED_ARRAY_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

/* Default elements per shard when the caller passes 0. */
#ifndef SCL_SHARDED_ARRAY_DEFAULT_SHARD
#define SCL_SHARDED_ARRAY_DEFAULT_SHARD 256u
#endif

/*
 * Initialise an empty sharded array of `elem_size`-byte elements with
 * `shard_len` elements per shard (0 => SCL_SHARDED_ARRAY_DEFAULT_SHARD). No
 * memory is reserved until the first append. Pick shard_len so a shard is a
 * convenient multiple of the cache line: a shard occupies
 * shard_len * elem_size bytes.
 */
scl_error_t scl_sharded_array_init(scl_allocator_t *alloc, scl_sharded_array_t *sa,
                                   size_t elem_size, size_t shard_len) SCL_WARN_UNUSED;

/*
 * Append a copy of *elem; the assigned index is returned in *out_index (may be
 * NULL). Indices are dense (0,1,2,...) and stable for the array's lifetime.
 * Grows by exactly one shard when the current one is full — existing elements
 * are never moved.
 */
scl_error_t scl_sharded_array_append(scl_sharded_array_t *sa, const void *elem,
                                     size_t *out_index) SCL_WARN_UNUSED;

/* Element at `index`, or NULL if out of range. O(1). The returned pointer stays
 * valid across subsequent appends (shards never move once allocated). */
static inline void *scl_sharded_array_get(const scl_sharded_array_t *sa, size_t index)
{
    if (scl_unlikely(!sa || index >= sa->count)) return NULL;
    return sa->shards[index / sa->shard_len] + (index % sa->shard_len) * sa->elem_size;
}

static inline size_t scl_sharded_array_count(const scl_sharded_array_t *sa)
{
    return sa ? sa->count : 0;
}

void scl_sharded_array_destroy(scl_sharded_array_t *sa);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
