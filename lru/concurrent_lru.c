#include "concurrent_lru.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static size_t default_hash(const void *key, size_t len)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t h = 5381;
    for (size_t i = 0; i < len; i++) h = ((h << 5) + h) + p[i];
    return h;
}

static size_t index_of(scl_atomic_lru_t *cache, const void *key)
{
    return cache->key_hash(key, cache->key_size) % cache->index_capacity;
}

static bool key_equal(scl_atomic_lru_t *cache, const void *a, const void *b)
{
    return memcmp(a, b, cache->key_size) == 0;
}

static void detach(scl_atomic_lru_t *cache, scl_atomic_lru_node_t *n)
{
    if (n->prev) n->prev->next = n->next;
    else cache->head = n->next;
    if (n->next) n->next->prev = n->prev;
    else cache->tail = n->prev;
}

static void push_front(scl_atomic_lru_t *cache, scl_atomic_lru_node_t *n)
{
    n->prev = NULL;
    n->next = cache->head;
    if (cache->head) cache->head->prev = n;
    cache->head = n;
    if (!cache->tail) cache->tail = n;
}

static void move_to_front(scl_atomic_lru_t *cache, scl_atomic_lru_node_t *n)
{
    detach(cache, n);
    push_front(cache, n);
}

scl_error_t scl_atomic_lru_init(scl_allocator_t *alloc, scl_atomic_lru_t *cache, size_t key_size, size_t value_size,
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
    cache->index = scl_calloc(alloc, cache->index_capacity, sizeof(scl_atomic_lru_node_t *), alignof(max_align_t));
    if (!cache->index) return SCL_ERR_OUT_OF_MEMORY;
    scl_spinlock_init(&cache->lock);
    return SCL_OK;
}

void scl_atomic_lru_destroy(scl_allocator_t *alloc, scl_atomic_lru_t *cache)
{
    if (!cache) return;
    scl_spinlock_lock(&cache->lock);
    scl_atomic_lru_node_t *cur = cache->head;
    while (cur) {
        scl_atomic_lru_node_t *next = cur->next;
        scl_free(alloc, cur->key);
        scl_free(alloc, cur->value);
        scl_free(alloc, cur);
        cur = next;
    }
    scl_free(alloc, cache->index);
    cache->head = cache->tail = NULL;
    cache->index = NULL;
    atomic_store_explicit(&cache->count, 0, memory_order_relaxed);
    scl_spinlock_unlock(&cache->lock);
}

scl_error_t scl_atomic_lru_put(scl_allocator_t *alloc, scl_atomic_lru_t *cache, const void *key, const void *value)
{
    if (!cache || !key || !value) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&cache->lock);
    size_t idx = index_of(cache, key);
    scl_atomic_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal(cache, cur->key, key)) {
            memcpy(cur->value, value, cache->value_size);
            move_to_front(cache, cur);
            scl_spinlock_unlock(&cache->lock);
            return SCL_OK;
        }
        cur = cur->next;
    }
    scl_atomic_lru_node_t *n = scl_alloc(alloc, sizeof(scl_atomic_lru_node_t), alignof(max_align_t));
    if (!n) { scl_spinlock_unlock(&cache->lock); return SCL_ERR_OUT_OF_MEMORY; }
    n->key = scl_alloc(alloc, cache->key_size, alignof(max_align_t));
    n->value = scl_alloc(alloc, cache->value_size, alignof(max_align_t));
    if (!n->key || !n->value) {
        scl_free(alloc, n->key); scl_free(alloc, n->value); scl_free(alloc, n);
        scl_spinlock_unlock(&cache->lock);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(n->key, key, cache->key_size);
    memcpy(n->value, value, cache->value_size);
    n->prev = n->next = NULL;
    if (atomic_load_explicit(&cache->count, memory_order_relaxed) == cache->capacity) {
        scl_atomic_lru_node_t *evict = cache->tail;
        detach(cache, evict);
        size_t eidx = index_of(cache, evict->key);
        scl_atomic_lru_node_t **ep = &cache->index[eidx];
        while (*ep) {
            if (*ep == evict) { *ep = evict->next; break; }
            ep = &(*ep)->next;
        }
        scl_free(alloc, evict->key); scl_free(alloc, evict->value); scl_free(alloc, evict);
        atomic_fetch_sub_explicit(&cache->count, 1, memory_order_relaxed);
    }
    push_front(cache, n);
    n->next = cache->index[idx];
    cache->index[idx] = n;
    atomic_fetch_add_explicit(&cache->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&cache->lock);
    return SCL_OK;
}

scl_error_t scl_atomic_lru_get(scl_atomic_lru_t *cache, const void *key, void *out_value)
{
    if (!cache || !key || !out_value) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&cache->lock);
    size_t idx = index_of(cache, key);
    scl_atomic_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal(cache, cur->key, key)) {
            memcpy(out_value, cur->value, cache->value_size);
            move_to_front(cache, cur);
            scl_spinlock_unlock(&cache->lock);
            return SCL_OK;
        }
        cur = cur->next;
    }
    scl_spinlock_unlock(&cache->lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_atomic_lru_contains(const scl_atomic_lru_t *cache, const void *key)
{
    if (!cache || !key) return false;
    scl_spinlock_lock((scl_spinlock_t *)&cache->lock);
    size_t idx = index_of((scl_atomic_lru_t *)cache, key);
    scl_atomic_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal((scl_atomic_lru_t *)cache, cur->key, key)) {
            scl_spinlock_unlock((scl_spinlock_t *)&cache->lock);
            return true;
        }
        cur = cur->next;
    }
    scl_spinlock_unlock((scl_spinlock_t *)&cache->lock);
    return false;
}

scl_error_t scl_atomic_lru_remove(scl_allocator_t *alloc, scl_atomic_lru_t *cache, const void *key)
{
    if (!cache || !key) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&cache->lock);
    size_t idx = index_of(cache, key);
    scl_atomic_lru_node_t **pp = &cache->index[idx];
    scl_atomic_lru_node_t *cur = cache->index[idx];
    while (cur) {
        if (key_equal(cache, cur->key, key)) {
            *pp = cur->next;
            detach(cache, cur);
            scl_free(alloc, cur->key);
            scl_free(alloc, cur->value);
            scl_free(alloc, cur);
            atomic_fetch_sub_explicit(&cache->count, 1, memory_order_relaxed);
            scl_spinlock_unlock(&cache->lock);
            return SCL_OK;
        }
        pp = &cur->next;
        cur = cur->next;
    }
    scl_spinlock_unlock(&cache->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_atomic_lru_count(const scl_atomic_lru_t *cache)
{
    return cache ? atomic_load_explicit(&cache->count, memory_order_relaxed) : 0;
}
