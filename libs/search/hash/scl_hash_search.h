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

/* Hash-table search. O(1) avg. Open-addressing with Robin Hood probing.
 *
 * Security: uses a randomized hash seed (call scl_search_ht_seed() at
 * program start) to defeat hash-flooding DoS attacks. */

#ifndef SCL_SEARCH_HASH_SEARCH_H
#define SCL_SEARCH_HASH_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef struct {
  char *key;
  void *value;
  bool occupied;
  bool deleted;
} scl_search_ht_entry_t;

typedef struct {
  scl_search_ht_entry_t *entries;
  size_t capacity;
  size_t mask;
  size_t count;
  scl_allocator_t *alloc;
} scl_search_ht_t;

/* Set a nonzero seed before creating any table. Zero restores a
 * compile-time default. Must be called before scl_search_ht_init. */
void scl_search_ht_seed(size_t seed);

scl_error_t scl_search_ht_init(scl_allocator_t *alloc,
                               scl_search_ht_t **SCL_RESTRICT ht,
                               size_t capacity) SCL_WARN_UNUSED;
scl_error_t scl_search_ht_insert(scl_search_ht_t *ht, const char *key,
                                 void *value) SCL_WARN_UNUSED;
scl_error_t scl_search_ht_search(const scl_search_ht_t *ht, const char *key,
                                 void **SCL_RESTRICT out_value) SCL_WARN_UNUSED;
scl_error_t scl_search_ht_delete(scl_search_ht_t *ht,
                                 const char *key) SCL_WARN_UNUSED;
void scl_search_ht_destroy(scl_search_ht_t *ht);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_SEARCH_HASH_SEARCH_H */
