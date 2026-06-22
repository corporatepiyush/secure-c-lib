#include "scl_lru.h"
#include <string.h>

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
    if (!cache) return SCL_ERR_NULL_PTR;
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

    cache->index_capacity = capacity * 2;
    cache->index = scl_calloc(alloc, cache->index_capacity, sizeof(scl_lru_node_t *), alignof(max_align_t));
    if (!cache->index) return SCL_ERR_OUT_OF_MEMORY;

    return SCL_OK;
}

void scl_lru_destroy(scl_allocator_t *alloc, scl_lru_t *cache)
{
    if (!cache) return;
    scl_lru_node_t *cur = cache->head;
    while (cur) {
        scl_lru_node_t *next = cur->next;
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
        if (memcmp(cur->key, key, cache->key_size) == 0)
            return cur;
    }
    return NULL;
}

static void scl_lru_evict(scl_allocator_t *alloc, scl_lru_t *cache)
{
    if (!cache->tail) return;
    scl_lru_node_t *old = cache->tail;
    scl_lru_detach(cache, old);
    scl_free(alloc, old->key);
    scl_free(alloc, old->value);
    scl_free(alloc, old);
    cache->count--;
}

scl_error_t scl_lru_put(scl_allocator_t *alloc, scl_lru_t *cache, const void *key, const void *value)
{
    if (!cache || !key || !value) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (node) {
        memcpy(node->value, value, cache->value_size);
        scl_lru_move_to_front(cache, node);
        return SCL_OK;
    }

    if (cache->count == cache->capacity)
        scl_lru_evict(alloc, cache);

    node = scl_alloc(alloc, sizeof(scl_lru_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;

    node->key = scl_alloc(alloc, cache->key_size, alignof(max_align_t));
    node->value = scl_alloc(alloc, cache->value_size, alignof(max_align_t));
    if (!node->key || !node->value) {
        scl_free(alloc, node->key);
        scl_free(alloc, node->value);
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(node->key, key, cache->key_size);
    memcpy(node->value, value, cache->value_size);

    node->prev = NULL;
    node->next = cache->head;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
    cache->count++;
    return SCL_OK;
}

scl_error_t scl_lru_get(scl_lru_t *cache, const void *key, void *out_value)
{
    if (!cache || !key || !out_value) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (!node) return SCL_ERR_NOT_FOUND;

    memcpy(out_value, node->value, cache->value_size);
    scl_lru_move_to_front(cache, node);
    return SCL_OK;
}

bool scl_lru_contains(const scl_lru_t *cache, const void *key)
{
    if (!cache || !key) return false;
    return scl_lru_find_node((scl_lru_t *)cache, key) != NULL;
}

scl_error_t scl_lru_remove(scl_allocator_t *alloc, scl_lru_t *cache, const void *key)
{
    if (!cache || !key) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (!node) return SCL_ERR_NOT_FOUND;

    scl_lru_detach(cache, node);
    scl_free(alloc, node->key);
    scl_free(alloc, node->value);
    scl_free(alloc, node);
    cache->count--;
    return SCL_OK;
}

void scl_lru_clear(scl_allocator_t *alloc, scl_lru_t *cache)
{
    if (!cache) return;
    scl_lru_node_t *cur = cache->head;
    while (cur) {
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
