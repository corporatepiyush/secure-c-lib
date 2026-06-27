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

/* deque data structure. */

#ifndef SCL_DEQUE_H
#define SCL_DEQUE_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t mask;
    size_t head;
    size_t count;
} scl_deque_t;

scl_error_t scl_deque_init(scl_allocator_t *SCL_RESTRICT alloc, scl_deque_t *SCL_RESTRICT deque, size_t element_size, size_t initial_capacity) SCL_WARN_UNUSED;
void        scl_deque_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_deque_t *SCL_RESTRICT deque);
scl_error_t scl_deque_push_front(scl_allocator_t *SCL_RESTRICT alloc, scl_deque_t *SCL_RESTRICT deque, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_deque_push_back(scl_allocator_t *SCL_RESTRICT alloc, scl_deque_t *SCL_RESTRICT deque, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_deque_pop_front(scl_deque_t *SCL_RESTRICT deque, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_deque_pop_back(scl_deque_t *SCL_RESTRICT deque, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_deque_peek_front(const scl_deque_t *SCL_RESTRICT deque, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_deque_peek_back(const scl_deque_t *SCL_RESTRICT deque, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_deque_count(const scl_deque_t *SCL_RESTRICT deque);
SCL_PURE bool        scl_deque_empty(const scl_deque_t *SCL_RESTRICT deque);

scl_error_t scl_deque_search(const scl_deque_t *deque, const void *key,
                             int (*cmp)(const void *, const void *),
                             size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
