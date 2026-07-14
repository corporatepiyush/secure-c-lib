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

/* Thread-safe bloom data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_BLOOM_H
#define SCL_CONCURRENT_BLOOM_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_bloom_hash_t)(const void *data, size_t len, size_t seed);

typedef struct {
  atomic_uchar *bits;
  size_t bit_count;
  size_t byte_count;
  size_t num_hashes;
  scl_bloom_hash_t hash_func;
  atomic_size_t inserted SCL_CACHE_ALIGNED;
} scl_concurrent_bloom_t;

scl_error_t scl_cbloom_init(scl_allocator_t *alloc, scl_concurrent_bloom_t *bf,
                            size_t expected_items, double false_positive_rate,
                            scl_bloom_hash_t hash_func) SCL_WARN_UNUSED;
void scl_cbloom_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                        scl_concurrent_bloom_t *SCL_RESTRICT bf);
scl_error_t scl_cbloom_insert(scl_concurrent_bloom_t *SCL_RESTRICT bf,
                              const void *SCL_RESTRICT data,
                              size_t len) SCL_WARN_UNUSED;
SCL_PURE bool
scl_cbloom_maybe_contains(const scl_concurrent_bloom_t *SCL_RESTRICT bf,
                          const void *SCL_RESTRICT data, size_t len);
void scl_cbloom_clear(scl_concurrent_bloom_t *SCL_RESTRICT bf);
SCL_PURE size_t scl_cbloom_count(const scl_concurrent_bloom_t *SCL_RESTRICT bf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
