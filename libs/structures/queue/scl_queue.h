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

/* queue data structure. */

#ifndef SCL_QUEUE_H
#define SCL_QUEUE_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t mask;         /* capacity - 1 (power-of-2) */
    size_t head;
    size_t tail;
    size_t count;
} scl_queue_t;

scl_error_t scl_queue_init(scl_allocator_t *SCL_RESTRICT alloc, scl_queue_t *SCL_RESTRICT queue, size_t element_size, size_t initial_capacity) SCL_WARN_UNUSED;
void        scl_queue_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_queue_t *SCL_RESTRICT queue);
scl_error_t scl_queue_enqueue(scl_allocator_t *SCL_RESTRICT alloc, scl_queue_t *SCL_RESTRICT queue, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_queue_dequeue(scl_queue_t *SCL_RESTRICT queue, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_queue_peek(const scl_queue_t *SCL_RESTRICT queue, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_queue_count(const scl_queue_t *SCL_RESTRICT queue);
SCL_PURE bool        scl_queue_empty(const scl_queue_t *SCL_RESTRICT queue);

scl_error_t scl_queue_search(const scl_queue_t *queue, const void *key,
                             int (*cmp)(const void *, const void *),
                             size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
