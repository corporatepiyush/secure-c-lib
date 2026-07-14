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

/* Trie search. O(k) per lookup. Prefix-tree for dictionary/autocomplete.
 *
 * NOTE: Each node uses 256 pointers (~2KB on 64-bit). For large dictionaries
 * consider using the radix-tree variant or limiting the alphabet size.
 * This implementation caps total node count to prevent unbounded memory growth.
 */

#ifndef SCL_SEARCH_TRIE_SEARCH_H
#define SCL_SEARCH_TRIE_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "scl_stdbool.h"

#define SCL_SEARCH_TRIE_ALPHABET_SIZE 256
#define SCL_SEARCH_TRIE_MAX_NODES 65536 /* Cap to ~128MB max */

typedef struct scl_search_trie_node {
  struct scl_search_trie_node *children[SCL_SEARCH_TRIE_ALPHABET_SIZE];
  bool is_end;
} scl_search_trie_node_t;

typedef struct {
  scl_search_trie_node_t *root;
  scl_allocator_t *alloc;
  size_t node_count; /* track for memory cap */
  size_t node_cap;   /* SCL_SEARCH_TRIE_MAX_NODES */
} scl_search_trie_t;

scl_error_t
scl_search_trie_init(scl_allocator_t *alloc,
                     scl_search_trie_t **SCL_RESTRICT trie) SCL_WARN_UNUSED;
scl_error_t scl_search_trie_insert(scl_search_trie_t *trie,
                                   const char *word) SCL_WARN_UNUSED;
bool scl_search_trie_search(const scl_search_trie_t *trie,
                            const char *word) SCL_PURE;
bool scl_search_trie_starts_with(const scl_search_trie_t *trie,
                                 const char *prefix) SCL_PURE;
scl_error_t scl_search_trie_delete(scl_search_trie_t *trie,
                                   const char *word) SCL_WARN_UNUSED;
void scl_search_trie_destroy(scl_search_trie_t *trie);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
