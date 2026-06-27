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

#include "scl_lru.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static size_t scl_lru_default_hash(const void *key, size_t key_size)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t h = 5381;
    for (size_t i = 0; i < key_size; i++)
        h = ((h << 5) + h) + p[i];
    return h;
}

scl_error_t scl_lru_init(scl_allocator_t *alloc, scl_lru_t *cache, size_t key_size, size_t value_size, size_t capacity)
{
    if (scl_unlikely(!cache)) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || capacity == 0)
        return SCL_ERR_INVALID_ARG;

    cache->head = NULL;
    cache->tail = NULL;
    cache->capacity = capacity;
    cache->count = 0;
    cache->key_size = key_size;
    cache->value_size = value_size;
    cache->key_cmp = NULL;
    cache->key_hash = scl_lru_default_hash;

    if (scl_unlikely(capacity > SIZE_MAX / 2)) return SCL_ERR_SIZE_OVERFLOW;
    cache->index_capacity = capacity * 2;
    cache->index = scl_calloc(alloc, cache->index_capacity, sizeof(scl_lru_node_t *), alignof(max_align_t));
    if (scl_unlikely(!cache->index)) return SCL_ERR_OUT_OF_MEMORY;

    return SCL_OK;
}

void scl_lru_destroy(scl_allocator_t *alloc, scl_lru_t *cache)
{
    if (scl_unlikely(!cache)) return;
    scl_lru_node_t *cur = cache->head;
    while (scl_likely(cur)) {
        scl_lru_node_t *next = cur->next;
        scl_secure_zero(cur->key,   cache->key_size);
        scl_secure_zero(cur->value, cache->value_size);
        scl_free(alloc, cur->key);
        scl_free(alloc, cur->value);
        scl_free(alloc, cur);
        cur = next;
    }
    scl_free(alloc, cache->index);
    cache->head = cache->tail = NULL;
    cache->index = NULL;
    cache->count = 0;
}

static void scl_lru_detach(scl_lru_t *cache, scl_lru_node_t *node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        cache->head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        cache->tail = node->prev;
}

static void scl_lru_move_to_front(scl_lru_t *cache, scl_lru_node_t *node)
{
    scl_lru_detach(cache, node);
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
}

static scl_lru_node_t *scl_lru_find_node(scl_lru_t *cache, const void *key)
{
    for (scl_lru_node_t *cur = cache->head; cur; cur = cur->next) {
        if (scl_memcmp(cur->key, key, cache->key_size) == 0)
            return cur;
    }
    return NULL;
}

static void scl_lru_evict(scl_allocator_t *alloc, scl_lru_t *cache)
{
    if (scl_unlikely(!cache->tail)) return;
    scl_lru_node_t *old = cache->tail;
    scl_lru_detach(cache, old);
    scl_secure_zero(old->key,   cache->key_size);
    scl_secure_zero(old->value, cache->value_size);
    scl_free(alloc, old->key);
    scl_free(alloc, old->value);
    scl_free(alloc, old);
    cache->count--;
}

scl_error_t scl_lru_put(scl_allocator_t *alloc, scl_lru_t *cache, const void  *SCL_RESTRICT key, const void *value)
{
    if (scl_unlikely(!cache || !key || !value)) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (node) {
        scl_memcpy(node->value, value, cache->value_size);
        scl_lru_move_to_front(cache, node);
        return SCL_OK;
    }

    if (cache->count == cache->capacity)
        scl_lru_evict(alloc, cache);

    node = scl_alloc(alloc, sizeof(scl_lru_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;

    node->key = scl_alloc(alloc, cache->key_size, alignof(max_align_t));
    node->value = scl_alloc(alloc, cache->value_size, alignof(max_align_t));
    if (scl_unlikely(!node->key || !node->value)) {
        scl_free(alloc, node->key);
        scl_free(alloc, node->value);
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    scl_memcpy(node->key, key, cache->key_size);
    scl_memcpy(node->value, value, cache->value_size);

    node->prev = NULL;
    node->next = cache->head;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
    cache->count++;
    return SCL_OK;
}

scl_error_t scl_lru_get(scl_lru_t *cache, const void *key, void  *SCL_RESTRICT out_value)
{
    if (scl_unlikely(!cache || !key || !out_value)) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (scl_unlikely(!node)) return SCL_ERR_NOT_FOUND;

    scl_memcpy(out_value, node->value, cache->value_size);
    scl_lru_move_to_front(cache, node);
    return SCL_OK;
}

bool scl_lru_contains(const scl_lru_t *cache, const void *key)
{
    if (scl_unlikely(!cache || !key)) return false;
    return scl_lru_find_node((scl_lru_t *)cache, key) != NULL;
}

scl_error_t scl_lru_remove(scl_allocator_t *alloc, scl_lru_t *cache, const void  *SCL_RESTRICT key)
{
    if (scl_unlikely(!cache || !key)) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (scl_unlikely(!node)) return SCL_ERR_NOT_FOUND;

    scl_lru_detach(cache, node);
    scl_free(alloc, node->key);
    scl_free(alloc, node->value);
    scl_free(alloc, node);
    cache->count--;
    return SCL_OK;
}

void scl_lru_clear(scl_allocator_t *alloc, scl_lru_t *cache)
{
    if (scl_unlikely(!cache)) return;
    scl_lru_node_t *cur = cache->head;
    while (scl_likely(cur)) {
        scl_lru_node_t *next = cur->next;
        scl_free(alloc, cur->key);
        scl_free(alloc, cur->value);
        scl_free(alloc, cur);
        cur = next;
    }
    cache->head = cache->tail = NULL;
    cache->count = 0;
}

size_t scl_lru_count(const scl_lru_t *cache) { return cache ? cache->count : 0; }
