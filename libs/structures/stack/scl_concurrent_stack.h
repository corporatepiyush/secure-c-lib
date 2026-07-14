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

/* Thread-safe stack data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_STACK_H
#define SCL_CONCURRENT_STACK_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_stack_node {
  struct scl_concurrent_stack_node *next;
  /* Flexible array member for element data — one allocation per node */
  unsigned char data[];
} scl_concurrent_stack_node_t;

typedef struct {
  _Atomic scl_tagged_ptr_t top SCL_CACHE_ALIGNED;
  size_t element_size;
  atomic_size_t count SCL_CACHE_ALIGNED;
} scl_concurrent_stack_t;

scl_error_t scl_cstack_init(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_concurrent_stack_t *SCL_RESTRICT stack,
                            size_t element_size) SCL_WARN_UNUSED;
void scl_cstack_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                        scl_concurrent_stack_t *SCL_RESTRICT stack);
scl_error_t scl_cstack_push(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_concurrent_stack_t *SCL_RESTRICT stack,
                            const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cstack_pop(scl_allocator_t *SCL_RESTRICT alloc,
                           scl_concurrent_stack_t *SCL_RESTRICT stack,
                           void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t
scl_cstack_count(const scl_concurrent_stack_t *SCL_RESTRICT stack);
SCL_PURE bool
scl_cstack_empty(const scl_concurrent_stack_t *SCL_RESTRICT stack);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
