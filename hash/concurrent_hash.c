#include "concurrent_hash.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void bucket_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ volatile("yield");
    }
}

static inline void bucket_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static size_t default_hash(const void *key, size_t len)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t h = 5381;
    for (size_t i = 0; i < len; i++)
        h = ((h << 5) + h) + p[i];
    return h;
}

scl_error_t scl_concurrent_hash_init(scl_concurrent_hash_t *ht, size_t key_size, size_t value_size,
                                     size_t bucket_count, scl_concurrent_hash_func_t hf)
{
    if (!ht) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || bucket_count == 0) return SCL_ERR_INVALID_ARG;
    ht->entries = calloc(bucket_count, sizeof(scl_concurrent_hash_entry_t *));
    if (!ht->entries) return SCL_ERR_OUT_OF_MEMORY;
    ht->buckets = calloc(bucket_count, sizeof(scl_concurrent_hash_bucket_t));
    if (!ht->buckets) {
        free(ht->entries);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < bucket_count; i++)
        atomic_flag_clear(&ht->buckets[i].lock);
    ht->bucket_count = bucket_count;
    atomic_init(&ht->count, 0);
    ht->hash_func = hf ? hf : default_hash;
    ht->key_size = key_size;
    ht->value_size = value_size;
    return SCL_OK;
}

void scl_concurrent_hash_destroy(scl_concurrent_hash_t *ht)
{
    if (!ht) return;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        scl_concurrent_hash_entry_t *e = ht->entries[i];
        while (e) {
            scl_concurrent_hash_entry_t *next = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = next;
        }
    }
    free(ht->entries);
    free(ht->buckets);
    ht->entries = NULL;
    ht->buckets = NULL;
    ht->bucket_count = 0;
    atomic_store_explicit(&ht->count, 0, memory_order_relaxed);
}

scl_error_t scl_concurrent_hash_insert(scl_concurrent_hash_t *ht, const void *key, const void *value)
{
    if (!ht || !key || !value) return SCL_ERR_NULL_PTR;
    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    bucket_lock(&ht->buckets[idx].lock);
    scl_concurrent_hash_entry_t *e = ht->entries[idx];
    while (e) {
        if (memcmp(e->key, key, ht->key_size) == 0) {
            memcpy(e->value, value, ht->value_size);
            bucket_unlock(&ht->buckets[idx].lock);
            return SCL_OK;
        }
        e = e->next;
    }
    scl_concurrent_hash_entry_t *n = malloc(sizeof(scl_concurrent_hash_entry_t));
    if (!n) { bucket_unlock(&ht->buckets[idx].lock); return SCL_ERR_OUT_OF_MEMORY; }
    n->key = malloc(ht->key_size);
    n->value = malloc(ht->value_size);
    if (!n->key || !n->value) {
        free(n->key); free(n->value); free(n);
        bucket_unlock(&ht->buckets[idx].lock);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(n->key, key, ht->key_size);
    memcpy(n->value, value, ht->value_size);
    n->next = ht->entries[idx];
    ht->entries[idx] = n;
    atomic_fetch_add_explicit(&ht->count, 1, memory_order_relaxed);
    bucket_unlock(&ht->buckets[idx].lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_hash_get(const scl_concurrent_hash_t *ht, const void *key, void *out_value)
{
    if (!ht || !key || !out_value) return SCL_ERR_NULL_PTR;
    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    bucket_lock(&ht->buckets[idx].lock);
    scl_concurrent_hash_entry_t *e = ht->entries[idx];
    while (e) {
        if (memcmp(e->key, key, ht->key_size) == 0) {
            memcpy(out_value, e->value, ht->value_size);
            bucket_unlock(&ht->buckets[idx].lock);
            return SCL_OK;
        }
        e = e->next;
    }
    bucket_unlock(&ht->buckets[idx].lock);
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_concurrent_hash_remove(scl_concurrent_hash_t *ht, const void *key)
{
    if (!ht || !key) return SCL_ERR_NULL_PTR;
    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    bucket_lock(&ht->buckets[idx].lock);
    scl_concurrent_hash_entry_t **prev = &ht->entries[idx];
    scl_concurrent_hash_entry_t *e = ht->entries[idx];
    while (e) {
        if (memcmp(e->key, key, ht->key_size) == 0) {
            *prev = e->next;
            free(e->key);
            free(e->value);
            free(e);
            atomic_fetch_sub_explicit(&ht->count, 1, memory_order_relaxed);
            bucket_unlock(&ht->buckets[idx].lock);
            return SCL_OK;
        }
        prev = &e->next;
        e = e->next;
    }
    bucket_unlock(&ht->buckets[idx].lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_concurrent_hash_contains(const scl_concurrent_hash_t *ht, const void *key)
{
    if (!ht || !key) return false;
    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    bucket_lock(&ht->buckets[idx].lock);
    scl_concurrent_hash_entry_t *e = ht->entries[idx];
    while (e) {
        if (memcmp(e->key, key, ht->key_size) == 0) {
            bucket_unlock(&ht->buckets[idx].lock);
            return true;
        }
        e = e->next;
    }
    bucket_unlock(&ht->buckets[idx].lock);
    return false;
}

size_t scl_concurrent_hash_count(const scl_concurrent_hash_t *ht)
{
    return ht ? atomic_load_explicit(&ht->count, memory_order_relaxed) : 0;
}
