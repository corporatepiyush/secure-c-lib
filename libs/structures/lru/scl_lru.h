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

/* lru data structure. */

#ifndef SCL_LRU_H
#define SCL_LRU_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_lru_node {
    void *key;
    void *value;
    struct scl_lru_node *prev;
    struct scl_lru_node *next;
} scl_lru_node_t;

typedef struct {
    scl_lru_node_t *head;
    scl_lru_node_t *tail;
    size_t capacity;
    size_t count;
    size_t key_size;
    size_t value_size;
    struct scl_lru_node **index;
    size_t index_capacity;
    int (*key_cmp)(const void *, const void *);
    size_t (*key_hash)(const void *, size_t);
} scl_lru_t;

scl_error_t scl_lru_init(scl_allocator_t *alloc, scl_lru_t *cache, size_t key_size, size_t value_size,
                         size_t capacity) SCL_WARN_UNUSED;
void        scl_lru_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_lru_t *SCL_RESTRICT cache);
scl_error_t scl_lru_put(scl_allocator_t *SCL_RESTRICT alloc, scl_lru_t *SCL_RESTRICT cache, const void *SCL_RESTRICT key, const void *SCL_RESTRICT value) SCL_WARN_UNUSED;
scl_error_t scl_lru_get(scl_lru_t *SCL_RESTRICT cache, const void *SCL_RESTRICT key, void *SCL_RESTRICT out_value) SCL_WARN_UNUSED;
SCL_PURE bool        scl_lru_contains(const scl_lru_t *SCL_RESTRICT cache, const void *SCL_RESTRICT key);
scl_error_t scl_lru_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_lru_t *SCL_RESTRICT cache, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
void        scl_lru_clear(scl_allocator_t *SCL_RESTRICT alloc, scl_lru_t *SCL_RESTRICT cache);
SCL_PURE size_t      scl_lru_count(const scl_lru_t *SCL_RESTRICT cache);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
