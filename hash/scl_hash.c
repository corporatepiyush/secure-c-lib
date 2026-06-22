#include "scl_hash.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

size_t scl_hash_djb2(const void *key, size_t len)
{
    const unsigned char *p = (const unsigned char *)key;
    size_t h = 5381;
    for (size_t i = 0; i < len; i++)
        h = ((h << 5) + h) + p[i];
    return h;
}

bool scl_hash_eq_mem(const void *a, const void *b, size_t key_size)
{
    return memcmp(a, b, key_size) == 0;
}

scl_error_t scl_hash_init(scl_hash_t *ht, size_t key_size, size_t value_size,
                          size_t initial_buckets, scl_hash_func_t hf,
                          scl_hash_eq_func_t eq)
{
    if (!ht) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || initial_buckets == 0 || !hf)
        return SCL_ERR_INVALID_ARG;

    ht->buckets = calloc(initial_buckets, sizeof(scl_hash_entry_t *));
    if (!ht->buckets) return SCL_ERR_OUT_OF_MEMORY;

    ht->bucket_count = initial_buckets;
    ht->count = 0;
    ht->hash_func = hf;
    ht->eq_func = eq ? eq : scl_hash_eq_mem;
    ht->key_size = key_size;
    ht->value_size = value_size;
    ht->load_factor = 0.75f;
    return SCL_OK;
}

void scl_hash_destroy(scl_hash_t *ht)
{
    if (!ht) return;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        scl_hash_entry_t *e = ht->buckets[i];
        while (e) {
            scl_hash_entry_t *next = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = next;
        }
    }
    free(ht->buckets);
    ht->buckets = NULL;
    ht->count = 0;
}

static scl_error_t scl_hash_rehash(scl_hash_t *ht)
{
    size_t new_count = ht->bucket_count * 2;
    scl_hash_entry_t **new_buckets = calloc(new_count, sizeof(scl_hash_entry_t *));
    if (!new_buckets) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < ht->bucket_count; i++) {
        scl_hash_entry_t *e = ht->buckets[i];
        while (e) {
            scl_hash_entry_t *next = e->next;
            size_t idx = ht->hash_func(e->key, ht->key_size) % new_count;
            e->next = new_buckets[idx];
            new_buckets[idx] = e;
            e = next;
        }
    }

    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->bucket_count = new_count;
    return SCL_OK;
}

scl_error_t scl_hash_insert(scl_hash_t *ht, const void *key, const void *value)
{
    if (!ht || !key || !value) return SCL_ERR_NULL_PTR;

    if ((float)ht->count / (float)ht->bucket_count > ht->load_factor) {
        scl_error_t err = scl_hash_rehash(ht);
        if (err != SCL_OK) return err;
    }

    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;

    scl_hash_entry_t *e = ht->buckets[idx];
    while (e) {
        if (ht->eq_func(e->key, key, ht->key_size)) {
            memcpy(e->value, value, ht->value_size);
            return SCL_OK;
        }
        e = e->next;
    }

    e = malloc(sizeof(scl_hash_entry_t));
    if (!e) return SCL_ERR_OUT_OF_MEMORY;

    e->key = malloc(ht->key_size);
    e->value = malloc(ht->value_size);
    if (!e->key || !e->value) {
        free(e->key);
        free(e->value);
        free(e);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(e->key, key, ht->key_size);
    memcpy(e->value, value, ht->value_size);
    e->key_len = ht->key_size;
    e->value_size = ht->value_size;
    e->next = ht->buckets[idx];
    ht->buckets[idx] = e;
    ht->count++;
    return SCL_OK;
}

scl_error_t scl_hash_get(const scl_hash_t *ht, const void *key, void *out_value)
{
    if (!ht || !key || !out_value) return SCL_ERR_NULL_PTR;

    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    scl_hash_entry_t *e = ht->buckets[idx];
    while (e) {
        if (ht->eq_func(e->key, key, ht->key_size)) {
            memcpy(out_value, e->value, ht->value_size);
            return SCL_OK;
        }
        e = e->next;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_hash_remove(scl_hash_t *ht, const void *key)
{
    if (!ht || !key) return SCL_ERR_NULL_PTR;

    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    scl_hash_entry_t **prev = &ht->buckets[idx];
    scl_hash_entry_t *e = ht->buckets[idx];

    while (e) {
        if (ht->eq_func(e->key, key, ht->key_size)) {
            *prev = e->next;
            free(e->key);
            free(e->value);
            free(e);
            ht->count--;
            return SCL_OK;
        }
        prev = &e->next;
        e = e->next;
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_hash_contains(const scl_hash_t *ht, const void *key)
{
    if (!ht || !key) return false;
    size_t idx = ht->hash_func(key, ht->key_size) % ht->bucket_count;
    scl_hash_entry_t *e = ht->buckets[idx];
    while (e) {
        if (ht->eq_func(e->key, key, ht->key_size)) return true;
        e = e->next;
    }
    return false;
}

size_t scl_hash_count(const scl_hash_t *ht)
{
    return ht ? ht->count : 0;
}
