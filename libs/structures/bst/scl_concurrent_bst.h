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

/* Thread-safe bst data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_BST_H
#define SCL_CONCURRENT_BST_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_bst_node {
  struct scl_concurrent_bst_node *left;
  struct scl_concurrent_bst_node *right;
  unsigned char data[]; /* flexible array member: element_size bytes */
} scl_concurrent_bst_node_t;

typedef struct {
  scl_concurrent_bst_node_t *root;
  size_t element_size;
  atomic_size_t count;
  scl_cmp_func_t cmp;
  scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_bst_t;

scl_error_t scl_cbst_init(scl_allocator_t *alloc, scl_concurrent_bst_t *tree,
                          size_t element_size,
                          scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void scl_cbst_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                      scl_concurrent_bst_t *SCL_RESTRICT tree);
scl_error_t scl_cbst_insert(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_concurrent_bst_t *SCL_RESTRICT tree,
                            const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cbst_remove(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_concurrent_bst_t *SCL_RESTRICT tree,
                            const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool scl_cbst_contains(scl_concurrent_bst_t *SCL_RESTRICT tree,
                                const void *SCL_RESTRICT key);
scl_error_t scl_cbst_find(scl_concurrent_bst_t *SCL_RESTRICT tree,
                          const void *SCL_RESTRICT key,
                          void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t scl_cbst_count(const scl_concurrent_bst_t *SCL_RESTRICT tree);
SCL_PURE bool scl_cbst_empty(const scl_concurrent_bst_t *SCL_RESTRICT tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
