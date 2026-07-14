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

/* Thread-safe dlist data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_DLIST_H
#define SCL_CONCURRENT_DLIST_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_dlist_node {
  struct scl_concurrent_dlist_node *prev;
  struct scl_concurrent_dlist_node *next;
  unsigned char data[]; /* flexible array member: element_size bytes */
} scl_concurrent_dlist_node_t;

typedef struct {
  scl_concurrent_dlist_node_t *head;
  scl_concurrent_dlist_node_t *tail;
  size_t element_size;
  atomic_size_t count;
  scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_dlist_t;

scl_error_t scl_cdlist_init(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_concurrent_dlist_t *SCL_RESTRICT list,
                            size_t element_size) SCL_WARN_UNUSED;
void scl_cdlist_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                        scl_concurrent_dlist_t *SCL_RESTRICT list);
scl_error_t
scl_cdlist_push_front(scl_allocator_t *SCL_RESTRICT alloc,
                      scl_concurrent_dlist_t *SCL_RESTRICT list,
                      const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t
scl_cdlist_push_back(scl_allocator_t *SCL_RESTRICT alloc,
                     scl_concurrent_dlist_t *SCL_RESTRICT list,
                     const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cdlist_pop_front(scl_allocator_t *SCL_RESTRICT alloc,
                                 scl_concurrent_dlist_t *SCL_RESTRICT list,
                                 void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_cdlist_pop_back(scl_allocator_t *SCL_RESTRICT alloc,
                                scl_concurrent_dlist_t *SCL_RESTRICT list,
                                void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t
scl_cdlist_insert_at(scl_allocator_t *SCL_RESTRICT alloc,
                     scl_concurrent_dlist_t *SCL_RESTRICT list, size_t index,
                     const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cdlist_remove_at(scl_allocator_t *SCL_RESTRICT alloc,
                                 scl_concurrent_dlist_t *SCL_RESTRICT list,
                                 size_t index,
                                 void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t
scl_cdlist_count(const scl_concurrent_dlist_t *SCL_RESTRICT list);
SCL_PURE bool scl_cdlist_empty(const scl_concurrent_dlist_t *SCL_RESTRICT list);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
