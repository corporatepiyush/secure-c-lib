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

/* Thread-safe btree data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_BTREE_H
#define SCL_CONCURRENT_BTREE_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_BTREE_DEGREE 4

typedef struct scl_concurrent_btree_node {
  void **keys;
  void **values;
  struct scl_concurrent_btree_node **children;
  size_t count;
  bool leaf;
} scl_concurrent_btree_node_t;

typedef struct {
  scl_concurrent_btree_node_t *root;
  size_t key_size;
  size_t value_size;
  atomic_size_t count;
  scl_cmp_func_t cmp;
  int t;
  scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_btree_t;

scl_error_t scl_cbtree_init(scl_allocator_t *alloc,
                            scl_concurrent_btree_t *tree, size_t key_size,
                            size_t value_size, int degree,
                            scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void scl_cbtree_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                        scl_concurrent_btree_t *SCL_RESTRICT tree);
scl_error_t scl_cbtree_insert(scl_allocator_t *SCL_RESTRICT alloc,
                              scl_concurrent_btree_t *SCL_RESTRICT tree,
                              const void *SCL_RESTRICT key,
                              const void *SCL_RESTRICT value) SCL_WARN_UNUSED;
scl_error_t scl_cbtree_get(scl_concurrent_btree_t *SCL_RESTRICT tree,
                           const void *SCL_RESTRICT key,
                           void *SCL_RESTRICT out_value) SCL_WARN_UNUSED;
scl_error_t scl_cbtree_remove(scl_allocator_t *SCL_RESTRICT alloc,
                              scl_concurrent_btree_t *SCL_RESTRICT tree,
                              const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool scl_cbtree_contains(scl_concurrent_btree_t *SCL_RESTRICT tree,
                                  const void *SCL_RESTRICT key);
SCL_PURE size_t
scl_cbtree_count(const scl_concurrent_btree_t *SCL_RESTRICT tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
