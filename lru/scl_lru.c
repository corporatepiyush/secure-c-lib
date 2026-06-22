#include "scl_lru.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static int scl_lru_default_cmp(const void *a, const void *b)
{
    return memcmp(a, b, 0);
}

static size_t scl_lru_default_hash(const void *key, size_t key_size)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t h = 5381;
    for (size_t i = 0; i < key_size; i++)
        h = ((h << 5) + h) + p[i];
    return h;
}

scl_error_t scl_lru_init(scl_lru_t *cache, size_t key_size, size_t value_size, size_t capacity)
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
    cache->key_cmp = scl_lru_default_cmp;
    cache->key_hash = scl_lru_default_hash;

    cache->index_capacity = capacity * 2;
    cache->index = calloc(cache->index_capacity, sizeof(scl_lru_node_t *));
    if (!cache->index) return SCL_ERR_OUT_OF_MEMORY;

    return SCL_OK;
}

void scl_lru_destroy(scl_lru_t *cache)
{
    if (!cache) return;
    scl_lru_node_t *cur = cache->head;
    while (cur) {
        scl_lru_node_t *next = cur->next;
        free(cur->key);
        free(cur->value);
        free(cur);
        cur = next;
    }
    free(cache->index);
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

static void scl_lru_evict(scl_lru_t *cache)
{
    if (!cache->tail) return;
    scl_lru_node_t *old = cache->tail;
    scl_lru_detach(cache, old);
    free(old->key);
    free(old->value);
    free(old);
    cache->count--;
}

scl_error_t scl_lru_put(scl_lru_t *cache, const void *key, const void *value)
{
    if (!cache || !key || !value) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (node) {
        memcpy(node->value, value, cache->value_size);
        scl_lru_move_to_front(cache, node);
        return SCL_OK;
    }

    if (cache->count == cache->capacity)
        scl_lru_evict(cache);

    node = malloc(sizeof(scl_lru_node_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;

    node->key = malloc(cache->key_size);
    node->value = malloc(cache->value_size);
    if (!node->key || !node->value) {
        free(node->key);
        free(node->value);
        free(node);
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

scl_error_t scl_lru_remove(scl_lru_t *cache, const void *key)
{
    if (!cache || !key) return SCL_ERR_NULL_PTR;

    scl_lru_node_t *node = scl_lru_find_node(cache, key);
    if (!node) return SCL_ERR_NOT_FOUND;

    scl_lru_detach(cache, node);
    free(node->key);
    free(node->value);
    free(node);
    cache->count--;
    return SCL_OK;
}

void scl_lru_clear(scl_lru_t *cache)
{
    if (!cache) return;
    scl_lru_node_t *cur = cache->head;
    while (cur) {
        scl_lru_node_t *next = cur->next;
        free(cur->key);
        free(cur->value);
        free(cur);
        cur = next;
    }
    cache->head = cache->tail = NULL;
    cache->count = 0;
}

size_t scl_lru_count(const scl_lru_t *cache) { return cache ? cache->count : 0; }
