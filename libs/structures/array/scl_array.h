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

/* array data structure. */

#ifndef SCL_ARRAY_H
#define SCL_ARRAY_H

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
} scl_array_t;

scl_error_t scl_array_init(scl_allocator_t *SCL_RESTRICT alloc, scl_array_t *SCL_RESTRICT arr, size_t element_size, size_t initial_capacity) SCL_WARN_UNUSED;
void        scl_array_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_array_t *SCL_RESTRICT arr);
scl_error_t scl_array_push(scl_allocator_t *SCL_RESTRICT alloc, scl_array_t *SCL_RESTRICT arr, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_array_pop(scl_array_t *SCL_RESTRICT arr, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_array_get(const scl_array_t *SCL_RESTRICT arr, size_t index, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_array_set(scl_array_t *SCL_RESTRICT arr, size_t index, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_array_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_array_t *SCL_RESTRICT arr, size_t index, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_array_remove(scl_array_t *SCL_RESTRICT arr, size_t index, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_array_reserve(scl_allocator_t *SCL_RESTRICT alloc, scl_array_t *SCL_RESTRICT arr, size_t new_capacity) SCL_WARN_UNUSED;
scl_error_t scl_array_shrink(scl_allocator_t *SCL_RESTRICT alloc, scl_array_t *SCL_RESTRICT arr) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_array_count(const scl_array_t *SCL_RESTRICT arr);
SCL_PURE size_t      scl_array_capacity(const scl_array_t *SCL_RESTRICT arr);
SCL_PURE bool        scl_array_empty(const scl_array_t *SCL_RESTRICT arr);

scl_error_t scl_array_linear_search(const scl_array_t *arr, const void *key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *out_index) SCL_WARN_UNUSED;
scl_error_t scl_array_binary_search(const scl_array_t *arr, const void *key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
