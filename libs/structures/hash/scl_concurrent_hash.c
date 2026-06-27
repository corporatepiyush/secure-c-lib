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

/* Thread-safe hash data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_hash.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

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
    if (scl_unlikely(!ht)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key_size == 0 || value_size == 0 || bucket_count == 0)) return SCL_ERR_INVALID_ARG;
    size_t nb = scl_bit_ceil_sz(bucket_count);
    ht->entries = scl_calloc(alloc, nb, sizeof(scl_concurrent_hash_entry_t *), alignof(max_align_t));
    if (scl_unlikely(!ht->entries)) return SCL_ERR_OUT_OF_MEMORY;
    ht->buckets = scl_calloc(alloc, nb, sizeof(scl_concurrent_hash_bucket_t), alignof(max_align_t));
    if (scl_unlikely(!ht->buckets)) {
        scl_free(alloc, ht->entries);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < nb; i++)
        scl_spinlock_init(&ht->buckets[i].lock);
    ht->bucket_count = nb;
    ht->mask = nb - 1;
    atomic_init(&ht->count, 0);
    ht->hash_func = hf ? hf : default_hash;
    ht->key_size = key_size;
    ht->value_size = value_size;
    return SCL_OK;
}

void scl_chash_destroy(scl_allocator_t *alloc, scl_concurrent_hash_t *ht)
{
    if (scl_unlikely(!ht)) return;
    size_t ksz = ht->key_size;
    size_t vsz = ht->value_size;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        scl_concurrent_hash_entry_t *e = ht->entries[i];
        while (scl_likely(e)) {
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
    ht->mask = 0;
    atomic_store_explicit(&ht->count, 0, memory_order_relaxed);
}

scl_error_t scl_chash_insert(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, const void  *SCL_RESTRICT key, const void *value)
{
    if (scl_unlikely(!ht || !key || !value)) return SCL_ERR_NULL_PTR;
    size_t ksz = ht->key_size;
    size_t vsz = ht->value_size;
    scl_concurrent_hash_bucket_t *buckets = ht->buckets;
    scl_concurrent_hash_entry_t **entries = ht->entries;
    size_t idx = ht->hash_func(key, ksz)  & ht->mask;
    scl_spinlock_lock(&buckets[idx].lock);
    scl_concurrent_hash_entry_t *e = entries[idx];
    while (scl_likely(e)) {
        if (scl_memcmp(e->key, key, ksz) == 0) {
            scl_memcpy(e->value, value, vsz);
            scl_spinlock_unlock(&buckets[idx].lock);
            return SCL_OK;
        }
        e = e->next;
    }
    scl_concurrent_hash_entry_t *n = scl_alloc(alloc, sizeof(scl_concurrent_hash_entry_t), alignof(max_align_t));
    if (!n) { scl_spinlock_unlock(&buckets[idx].lock); return SCL_ERR_OUT_OF_MEMORY; }
    n->key = scl_alloc(alloc, ksz, alignof(max_align_t));
    n->value = scl_alloc(alloc, vsz, alignof(max_align_t));
    if (scl_unlikely(!n->key || !n->value)) {
        scl_free(alloc, n->key); scl_free(alloc, n->value); scl_free(alloc, n);
        scl_spinlock_unlock(&buckets[idx].lock);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(n->key, key, ksz);
    scl_memcpy(n->value, value, vsz);
    n->next = entries[idx];
    entries[idx] = n;
    atomic_fetch_add_explicit(&ht->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&ht->buckets[idx].lock);
    return SCL_OK;
}

scl_error_t scl_chash_get(const scl_concurrent_hash_t *ht, const void *key, void  *SCL_RESTRICT out_value)
{
    if (scl_unlikely(!ht || !key || !out_value)) return SCL_ERR_NULL_PTR;
    size_t ksz = ht->key_size;
    scl_concurrent_hash_bucket_t *buckets = ht->buckets;
    scl_concurrent_hash_entry_t **entries = ht->entries;
    size_t idx = ht->hash_func(key, ksz)  & ht->mask;
    scl_spinlock_lock(&buckets[idx].lock);
    scl_concurrent_hash_entry_t *e = entries[idx];
    while (scl_likely(e)) {
        if (scl_memcmp(e->key, key, ksz) == 0) {
            scl_memcpy(out_value, e->value, ht->value_size);
            scl_spinlock_unlock(&buckets[idx].lock);
            return SCL_OK;
        }
        e = e->next;
    }
    scl_spinlock_unlock(&buckets[idx].lock);
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_chash_remove(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, const void  *SCL_RESTRICT key)
{
    if (scl_unlikely(!ht || !key)) return SCL_ERR_NULL_PTR;
    size_t ksz = ht->key_size;
    size_t vsz = ht->value_size;
    scl_concurrent_hash_bucket_t *buckets = ht->buckets;
    scl_concurrent_hash_entry_t **entries = ht->entries;
    size_t idx = ht->hash_func(key, ksz)  & ht->mask;
    scl_spinlock_lock(&buckets[idx].lock);
    scl_concurrent_hash_entry_t **prev = &entries[idx];
    scl_concurrent_hash_entry_t *e = entries[idx];
    while (scl_likely(e)) {
        if (scl_memcmp(e->key, key, ksz) == 0) {
            *prev = e->next;
            scl_secure_zero(e->key,   ksz);
            scl_secure_zero(e->value, vsz);
            scl_free(alloc, e->key);
            scl_free(alloc, e->value);
            scl_free(alloc, e);
            atomic_fetch_sub_explicit(&ht->count, 1, memory_order_relaxed);
            scl_spinlock_unlock(&buckets[idx].lock);
            return SCL_OK;
        }
        prev = &e->next;
        e = e->next;
    }
    scl_spinlock_unlock(&buckets[idx].lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_chash_contains(const scl_concurrent_hash_t *ht, const void *key)
{
    if (scl_unlikely(!ht || !key)) return false;
    size_t ksz = ht->key_size;
    scl_concurrent_hash_bucket_t *buckets = ht->buckets;
    scl_concurrent_hash_entry_t **entries = ht->entries;
    size_t idx = ht->hash_func(key, ksz)  & ht->mask;
    scl_spinlock_lock(&buckets[idx].lock);
    scl_concurrent_hash_entry_t *e = entries[idx];
    while (scl_likely(e)) {
        if (scl_memcmp(e->key, key, ksz) == 0) {
            scl_spinlock_unlock(&buckets[idx].lock);
            return true;
        }
        e = e->next;
    }
    scl_spinlock_unlock(&buckets[idx].lock);
    return false;
}

size_t scl_chash_count(const scl_concurrent_hash_t *ht)
{
    return ht ? atomic_load_explicit(&ht->count, memory_order_relaxed) : 0;
}
