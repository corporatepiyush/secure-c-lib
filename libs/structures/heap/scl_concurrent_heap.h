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

/* Thread-safe heap data structure. Guarded by scl_spinlock_t. */

#ifndef SCL_CONCURRENT_HEAP_H
#define SCL_CONCURRENT_HEAP_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_heap_t;

scl_error_t scl_cheap_init(scl_allocator_t *alloc, scl_concurrent_heap_t *heap, size_t element_size,
                          size_t capacity, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_cheap_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_heap_t *SCL_RESTRICT heap);
scl_error_t scl_cheap_push(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_heap_t *SCL_RESTRICT heap, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cheap_pop(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_heap_t *SCL_RESTRICT heap, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_cheap_peek(scl_concurrent_heap_t *SCL_RESTRICT heap, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_cheap_count(const scl_concurrent_heap_t *SCL_RESTRICT heap);
SCL_PURE bool        scl_cheap_empty(const scl_concurrent_heap_t *SCL_RESTRICT heap);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
