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

/* avl data structure. */

#ifndef SCL_AVL_H
#define SCL_AVL_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_avl_node {
  void *data;
  struct scl_avl_node *left;
  struct scl_avl_node *right;
  int height;
} scl_avl_node_t;

typedef struct {
  scl_avl_node_t *root;
  size_t element_size;
  size_t count;
  scl_cmp_func_t cmp;
} scl_avl_t;

scl_error_t scl_avl_init(scl_allocator_t *SCL_RESTRICT alloc,
                         scl_avl_t *SCL_RESTRICT tree, size_t element_size,
                         scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void scl_avl_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                     scl_avl_t *SCL_RESTRICT tree);
scl_error_t scl_avl_insert(scl_allocator_t *SCL_RESTRICT alloc,
                           scl_avl_t *SCL_RESTRICT tree,
                           const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_avl_remove(scl_allocator_t *SCL_RESTRICT alloc,
                           scl_avl_t *SCL_RESTRICT tree,
                           const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool scl_avl_contains(const scl_avl_t *SCL_RESTRICT tree,
                               const void *SCL_RESTRICT key);
scl_error_t scl_avl_find(const scl_avl_t *SCL_RESTRICT tree,
                         const void *SCL_RESTRICT key,
                         void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_avl_min(const scl_avl_t *SCL_RESTRICT tree,
                        void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_avl_max(const scl_avl_t *SCL_RESTRICT tree,
                        void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t scl_avl_count(const scl_avl_t *SCL_RESTRICT tree);
SCL_PURE bool scl_avl_empty(const scl_avl_t *SCL_RESTRICT tree);

scl_error_t scl_avl_inorder(const scl_avl_t *SCL_RESTRICT tree,
                            scl_visit_func_t visit,
                            void *SCL_RESTRICT ctx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
