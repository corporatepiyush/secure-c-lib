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

/* skiplist data structure. */

#ifndef SCL_SKIPLIST_H
#define SCL_SKIPLIST_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_SKIPLIST_MAX_LEVEL 16

typedef struct scl_skiplist_node {
  void *data;
  struct scl_skiplist_node **forward;
  size_t level;
} scl_skiplist_node_t;

typedef struct {
  scl_skiplist_node_t *head;
  size_t element_size;
  size_t count;
  scl_cmp_func_t cmp;
  size_t level;
} scl_skiplist_t;

scl_error_t scl_skiplist_init(scl_allocator_t *alloc, scl_skiplist_t *sl,
                              size_t element_size,
                              scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void scl_skiplist_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                          scl_skiplist_t *SCL_RESTRICT sl);
scl_error_t
scl_skiplist_insert(scl_allocator_t *SCL_RESTRICT alloc,
                    scl_skiplist_t *SCL_RESTRICT sl,
                    const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_skiplist_remove(scl_allocator_t *SCL_RESTRICT alloc,
                                scl_skiplist_t *SCL_RESTRICT sl,
                                const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool scl_skiplist_contains(const scl_skiplist_t *SCL_RESTRICT sl,
                                    const void *SCL_RESTRICT key);
scl_error_t scl_skiplist_find(const scl_skiplist_t *SCL_RESTRICT sl,
                              const void *SCL_RESTRICT key,
                              void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t scl_skiplist_count(const scl_skiplist_t *SCL_RESTRICT sl);
SCL_PURE bool scl_skiplist_empty(const scl_skiplist_t *SCL_RESTRICT sl);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
