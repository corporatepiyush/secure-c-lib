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

/* ringbuf data structure. */

#ifndef SCL_RINGBUF_H
#define SCL_RINGBUF_H

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
    size_t count;
    bool overwrite;
} scl_ringbuf_t;

scl_error_t scl_ringbuf_init(scl_allocator_t *alloc, scl_ringbuf_t *rb, size_t element_size, size_t capacity,
                             bool overwrite) SCL_WARN_UNUSED;
void        scl_ringbuf_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_ringbuf_t *SCL_RESTRICT rb);
scl_error_t scl_ringbuf_push(scl_ringbuf_t *SCL_RESTRICT rb, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_ringbuf_pop(scl_ringbuf_t *SCL_RESTRICT rb, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_ringbuf_peek(const scl_ringbuf_t *SCL_RESTRICT rb, size_t index, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_ringbuf_count(const scl_ringbuf_t *SCL_RESTRICT rb);
SCL_PURE size_t      scl_ringbuf_capacity(const scl_ringbuf_t *SCL_RESTRICT rb);
SCL_PURE bool        scl_ringbuf_empty(const scl_ringbuf_t *SCL_RESTRICT rb);
bool        scl_ringbuf_full(const scl_ringbuf_t *SCL_RESTRICT rb);

scl_error_t scl_ringbuf_search(const scl_ringbuf_t *rb, const void *key,
                               int (*cmp)(const void *, const void *),
                               size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
