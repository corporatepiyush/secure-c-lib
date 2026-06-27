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

/* Thread-safe hash data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_HASH_H
#define SCL_CONCURRENT_HASH_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_hash_func_t)(const void *key, size_t len);

typedef struct scl_concurrent_hash_entry {
    void *key;
    void *value;
    struct scl_concurrent_hash_entry *next;
} scl_concurrent_hash_entry_t;

typedef struct {
    scl_spinlock_t lock;
} scl_concurrent_hash_bucket_t;

typedef struct {
    scl_concurrent_hash_entry_t **entries;
    scl_concurrent_hash_bucket_t *buckets;
    size_t bucket_count;
    size_t mask;
    atomic_size_t count SCL_CACHE_ALIGNED;
    scl_hash_func_t hash_func;
    size_t key_size;
    size_t value_size;
} scl_concurrent_hash_t;

scl_error_t scl_chash_init(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, size_t key_size, size_t value_size,
                          size_t bucket_count, scl_hash_func_t hf) SCL_WARN_UNUSED;
void        scl_chash_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_hash_t *SCL_RESTRICT ht);
scl_error_t scl_chash_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_hash_t *SCL_RESTRICT ht, const void *SCL_RESTRICT key, const void *SCL_RESTRICT value) SCL_WARN_UNUSED;
scl_error_t scl_chash_get(const scl_concurrent_hash_t *SCL_RESTRICT ht, const void *SCL_RESTRICT key, void *SCL_RESTRICT out_value) SCL_WARN_UNUSED;
scl_error_t scl_chash_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_hash_t *SCL_RESTRICT ht, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool        scl_chash_contains(const scl_concurrent_hash_t *SCL_RESTRICT ht, const void *SCL_RESTRICT key);
SCL_PURE size_t      scl_chash_count(const scl_concurrent_hash_t *SCL_RESTRICT ht);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
