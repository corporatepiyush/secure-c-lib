#include "concurrent_hash.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void bucket_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        scl_cpu_pause();
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

scl_error_t scl_chash_init(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, size_t key_size, size_t value_size,
                          size_t bucket_count, scl_hash_func_t hf)
{
    if (!ht) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || bucket_count == 0) return SCL_ERR_INVALID_ARG;
    ht->entries = scl_calloc(alloc, bucket_count, sizeof(scl_concurrent_hash_entry_t *), alignof(max_align_t));
    if (!ht->entries) return SCL_ERR_OUT_OF_MEMORY;
    ht->buckets = scl_calloc(alloc, bucket_count, sizeof(scl_concurrent_hash_bucket_t), alignof(max_align_t));
    if (!ht->buckets) {
        scl_free(alloc, ht->entries);
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

void scl_chash_destroy(scl_allocator_t *alloc, scl_concurrent_hash_t *ht)
{
    if (!ht) return;
    size_t ksz = ht->key_size;
    size_t vsz = ht->value_size;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        scl_concurrent_hash_entry_t *e = ht->entries[i];
        while (e) {
            scl_concurrent_hash_entry_t *next = e->next;
            if (e->key)   scl_secure_zero(e->key,   ksz);
            if (e->value) scl_secure_zero(e->value, vsz);
            scl_free(alloc, e->key);
            scl_free(alloc, e->value);
            scl_free(alloc, e);
            e = next;
        }
    }
    scl_free(alloc, ht->entries);
    scl_free(alloc, ht->buckets);
    ht->entries = NULL;
    ht->buckets = NULL;
    ht->bucket_count = 0;
    atomic_store_explicit(&ht->count, 0, memory_order_relaxed);
}

scl_error_t scl_chash_insert(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, const void *key, const void *value)
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
    scl_concurrent_hash_entry_t *n = scl_alloc(alloc, sizeof(scl_concurrent_hash_entry_t), alignof(max_align_t));
    if (!n) { bucket_unlock(&ht->buckets[idx].lock); return SCL_ERR_OUT_OF_MEMORY; }
    n->key = scl_alloc(alloc, ht->key_size, alignof(max_align_t));
    n->value = scl_alloc(alloc, ht->value_size, alignof(max_align_t));
    if (!n->key || !n->value) {
        scl_free(alloc, n->key); scl_free(alloc, n->value); scl_free(alloc, n);
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

scl_error_t scl_chash_get(const scl_concurrent_hash_t *ht, const void *key, void *out_value)
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

scl_error_t scl_chash_remove(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, const void *key)
{
    if (!ht || !key) return SCL_ERR_NULL_PTR;
    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    bucket_lock(&ht->buckets[idx].lock);
    scl_concurrent_hash_entry_t **prev = &ht->entries[idx];
    scl_concurrent_hash_entry_t *e = ht->entries[idx];
    while (e) {
        if (memcmp(e->key, key, ht->key_size) == 0) {
            *prev = e->next;
            scl_secure_zero(e->key,   ht->key_size);
            scl_secure_zero(e->value, ht->value_size);
            scl_free(alloc, e->key);
            scl_free(alloc, e->value);
            scl_free(alloc, e);
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

bool scl_chash_contains(const scl_concurrent_hash_t *ht, const void *key)
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

size_t scl_chash_count(const scl_concurrent_hash_t *ht)
{
    return ht ? atomic_load_explicit(&ht->count, memory_order_relaxed) : 0;
}
