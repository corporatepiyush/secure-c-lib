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

/* heap data structure. */

#ifndef SCL_HEAP_H
#define SCL_HEAP_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t count;
    scl_cmp_func_t cmp;
} scl_heap_t;

scl_error_t scl_heap_init(scl_allocator_t *alloc, scl_heap_t *heap, size_t element_size, size_t initial_capacity,
                          scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_heap_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_heap_t *SCL_RESTRICT heap);
scl_error_t scl_heap_push(scl_allocator_t *SCL_RESTRICT alloc, scl_heap_t *SCL_RESTRICT heap, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_heap_pop(scl_heap_t *SCL_RESTRICT heap, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_heap_peek(const scl_heap_t *SCL_RESTRICT heap, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_heap_count(const scl_heap_t *SCL_RESTRICT heap);
SCL_PURE bool        scl_heap_empty(const scl_heap_t *SCL_RESTRICT heap);

scl_error_t scl_heap_search(const scl_heap_t *heap, const void *key,
                            scl_cmp_func_t cmp,
                            size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
