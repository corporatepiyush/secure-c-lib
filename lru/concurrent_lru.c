#include "concurrent_lru.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ volatile("yield");
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static size_t default_hash(const void *key, size_t len)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t h = 5381;
    for (size_t i = 0; i < len; i++) h = ((h << 5) + h) + p[i];
    return h;
}

static size_t index_of(scl_concurrent_lru_t *cache, const void *key)
{
    return cache->key_hash(key, cache->key_size) % cache->index_capacity;
}

static bool key_equal(scl_concurrent_lru_t *cache, const void *a, const void *b)
{
    return memcmp(a, b, cache->key_size) == 0;
}

static void detach(scl_concurrent_lru_t *cache, scl_concurrent_lru_node_t *n)
{
    if (n->prev) n->prev->next = n->next;
    else cache->head = n->next;
    if (n->next) n->next->prev = n->prev;
    else cache->tail = n->prev;
}

static void push_front(scl_concurrent_lru_t *cache, scl_concurrent_lru_node_t *n)
{
    n->prev = NULL;
    n->next = cache->head;
    if (cache->head) cache->head->prev = n;
    cache->head = n;
    if (!cache->tail) cache->tail = n;
}

static void move_to_front(scl_concurrent_lru_t *cache, scl_concurrent_lru_node_t *n)
{
    detach(cache, n);
    push_front(cache, n);
}

scl_error_t scl_concurrent_lru_init(scl_concurrent_lru_t *cache, size_t key_size, size_t value_size,
                                    size_t capacity)
{
    if (!cache) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || capacity == 0) return SCL_ERR_INVALID_ARG;
    cache->head = cache->tail = NULL;
    cache->capacity = capacity;
    atomic_init(&cache->count, 0);
    cache->key_size = key_size;
    cache->value_size = value_size;
    cache->key_cmp = NULL;
    cache->key_hash = default_hash;
    cache->index_capacity = capacity * 2;
    cache->index = calloc(cache->index_capacity, sizeof(scl_concurrent_lru_node_t *));
    if (!cache->index) return SCL_ERR_OUT_OF_MEMORY;
    atomic_flag_clear(&cache->lock);
    return SCL_OK;
}

void scl_concurrent_lru_destroy(scl_concurrent_lru_t *cache)
{
    if (!cache) return;
    spin_lock(&cache->lock);
    scl_concurrent_lru_node_t *cur = cache->head;
    while (cur) {
        scl_concurrent_lru_node_t *next = cur->next;
        free(cur->key);
        free(cur->value);
        free(cur);
        cur = next;
    }
    free(cache->index);
    cache->head = cache->tail = NULL;
    cache->index = NULL;
    atomic_store_explicit(&cache->count, 0, memory_order_relaxed);
    spin_unlock(&cache->lock);
}

scl_error_t scl_concurrent_lru_put(scl_concurrent_lru_t *cache, const void *key, const void *value)
{
    if (!cache || !key || !value) return SCL_ERR_NULL_PTR;
    spin_lock(&cache->lock);
    size_t idx = index_of(cache, key);
    scl_concurrent_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal(cache, cur->key, key)) {
            memcpy(cur->value, value, cache->value_size);
            move_to_front(cache, cur);
            spin_unlock(&cache->lock);
            return SCL_OK;
        }
        cur = cur->next;
    }
    scl_concurrent_lru_node_t *n = malloc(sizeof(scl_concurrent_lru_node_t));
    if (!n) { spin_unlock(&cache->lock); return SCL_ERR_OUT_OF_MEMORY; }
    n->key = malloc(cache->key_size);
    n->value = malloc(cache->value_size);
    if (!n->key || !n->value) {
        free(n->key); free(n->value); free(n);
        spin_unlock(&cache->lock);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(n->key, key, cache->key_size);
    memcpy(n->value, value, cache->value_size);
    n->prev = n->next = NULL;
    if (atomic_load_explicit(&cache->count, memory_order_relaxed) == cache->capacity) {
        scl_concurrent_lru_node_t *evict = cache->tail;
        detach(cache, evict);
        size_t eidx = index_of(cache, evict->key);
        scl_concurrent_lru_node_t **ep = &cache->index[eidx];
        while (*ep) {
            if (*ep == evict) { *ep = evict->next; break; }
            ep = &(*ep)->next;
        }
        free(evict->key); free(evict->value); free(evict);
        atomic_fetch_sub_explicit(&cache->count, 1, memory_order_relaxed);
    }
    push_front(cache, n);
    n->next = cache->index[idx];
    cache->index[idx] = n;
    atomic_fetch_add_explicit(&cache->count, 1, memory_order_relaxed);
    spin_unlock(&cache->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_lru_get(scl_concurrent_lru_t *cache, const void *key, void *out_value)
{
    if (!cache || !key || !out_value) return SCL_ERR_NULL_PTR;
    spin_lock(&cache->lock);
    size_t idx = index_of(cache, key);
    scl_concurrent_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal(cache, cur->key, key)) {
            memcpy(out_value, cur->value, cache->value_size);
            move_to_front(cache, cur);
            spin_unlock(&cache->lock);
            return SCL_OK;
        }
        cur = cur->next;
    }
    spin_unlock(&cache->lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_concurrent_lru_contains(const scl_concurrent_lru_t *cache, const void *key)
{
    if (!cache || !key) return false;
    spin_lock((atomic_flag *)&cache->lock);
    size_t idx = index_of((scl_concurrent_lru_t *)cache, key);
    scl_concurrent_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal((scl_concurrent_lru_t *)cache, cur->key, key)) {
            spin_unlock((atomic_flag *)&cache->lock);
            return true;
        }
        cur = cur->next;
    }
    spin_unlock((atomic_flag *)&cache->lock);
    return false;
}

scl_error_t scl_concurrent_lru_remove(scl_concurrent_lru_t *cache, const void *key)
{
    if (!cache || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&cache->lock);
    size_t idx = index_of(cache, key);
    scl_concurrent_lru_node_t **pp = &cache->index[idx];
    scl_concurrent_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal(cache, cur->key, key)) {
            *pp = cur->next;
            detach(cache, cur);
            free(cur->key);
            free(cur->value);
            free(cur);
            atomic_fetch_sub_explicit(&cache->count, 1, memory_order_relaxed);
            spin_unlock(&cache->lock);
            return SCL_OK;
        }
        pp = &cur->next;
        cur = cur->next;
    }
    spin_unlock(&cache->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_concurrent_lru_count(const scl_concurrent_lru_t *cache)
{
    return cache ? atomic_load_explicit(&cache->count, memory_order_relaxed) : 0;
}
