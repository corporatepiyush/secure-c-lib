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

/* hash data structure. */

#ifndef SCL_HASH_H
#define SCL_HASH_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_hash_func_t)(const void *key, size_t len);
typedef bool (*scl_hash_eq_func_t)(const void *a, const void *b,
                                   size_t key_size);

/* Open-addressing hash table with inline key/value storage.
 * No per-entry allocations — flat arrays, cache-friendly.
 * Uses tombstone markers for deletion.
 */
typedef enum {
  SCL_HASH_EMPTY = 0,
  SCL_HASH_OCCUPIED = 1,
  SCL_HASH_TOMBSTONE = 2
} scl_hash_slot_state_t;

typedef struct {
  unsigned char *keys;           /* key_size * capacity flat array */
  unsigned char *values;         /* value_size * capacity flat array */
  scl_hash_slot_state_t *states; /* state per slot */
  size_t capacity;
  size_t mask; /* capacity - 1 (power-of-2) */
  size_t count;
  scl_hash_func_t hash_func;
  scl_hash_eq_func_t eq_func;
  uint64_t seed; /* random per-table seed (hash DoS mitigation) */
  size_t key_size;
  size_t value_size;
} scl_hash_t;

scl_error_t scl_hash_init(scl_allocator_t *alloc, scl_hash_t *ht,
                          size_t key_size, size_t value_size,
                          size_t initial_buckets, scl_hash_func_t hf,
                          scl_hash_eq_func_t eq) SCL_WARN_UNUSED;
void scl_hash_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                      scl_hash_t *SCL_RESTRICT ht);
scl_error_t scl_hash_insert(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_hash_t *SCL_RESTRICT ht,
                            const void *SCL_RESTRICT key,
                            const void *SCL_RESTRICT value) SCL_WARN_UNUSED;
scl_error_t scl_hash_get(const scl_hash_t *SCL_RESTRICT ht,
                         const void *SCL_RESTRICT key,
                         void *SCL_RESTRICT out_value) SCL_WARN_UNUSED;
scl_error_t scl_hash_remove(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_hash_t *SCL_RESTRICT ht,
                            const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool scl_hash_contains(const scl_hash_t *SCL_RESTRICT ht,
                                const void *SCL_RESTRICT key);
SCL_PURE size_t scl_hash_count(const scl_hash_t *SCL_RESTRICT ht);

size_t scl_hash_djb2(const void *SCL_RESTRICT key, size_t len);
bool scl_hash_eq_mem(const void *SCL_RESTRICT a, const void *SCL_RESTRICT b,
                     size_t key_size);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
