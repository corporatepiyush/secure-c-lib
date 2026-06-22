#include "scl_hash.h"
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

static SCL_COLD_PATH scl_error_t scl_hash_grow(scl_allocator_t *alloc, scl_hash_t *ht)
{
    size_t new_cap = ht->capacity == 0 ? 16 : ht->capacity * 2;
    size_t new_mask = new_cap - 1;

    size_t ksz = ht->key_size;
    size_t vsz = ht->value_size;

    unsigned char *new_keys = scl_alloc(alloc, new_cap * ksz, alignof(max_align_t));
    unsigned char *new_vals = scl_alloc(alloc, new_cap * vsz, alignof(max_align_t));
    scl_hash_slot_state_t *new_states = scl_alloc(alloc, new_cap * sizeof(scl_hash_slot_state_t), alignof(max_align_t));
    if (!new_keys || !new_vals || !new_states) {
        scl_free(alloc, new_keys);
        scl_free(alloc, new_vals);
        scl_free(alloc, new_states);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memset(new_states, 0, new_cap * sizeof(scl_hash_slot_state_t));

    /* Rehash all occupied entries into new table */
    scl_hash_func_t hf = ht->hash_func;
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->states[i] == SCL_HASH_OCCUPIED) {
            size_t idx = hf(ht->keys + i * ksz, ksz) & new_mask;
            while (new_states[idx] == SCL_HASH_OCCUPIED)
                idx = (idx + 1) & new_mask;
            memcpy(new_keys + idx * ksz, ht->keys + i * ksz, ksz);
            memcpy(new_vals + idx * vsz, ht->values + i * vsz, vsz);
            new_states[idx] = SCL_HASH_OCCUPIED;
        }
    }

    scl_free(alloc, ht->keys);
    scl_free(alloc, ht->values);
    scl_free(alloc, ht->states);

    ht->keys = new_keys;
    ht->values = new_vals;
    ht->states = new_states;
    ht->capacity = new_cap;
    ht->mask = new_mask;
    return SCL_OK;
}

scl_error_t scl_hash_init(scl_allocator_t *alloc, scl_hash_t *ht, size_t key_size, size_t value_size,
                          size_t initial_buckets, scl_hash_func_t hf,
                          scl_hash_eq_func_t eq)
{
    if (!ht) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || initial_buckets == 0 || !hf)
        return SCL_ERR_INVALID_ARG;

    size_t cap = scl_bit_ceil_sz(initial_buckets);
    size_t ksz = key_size, vsz = value_size;

    ht->keys = scl_calloc(alloc, cap, ksz, alignof(max_align_t));
    ht->values = scl_calloc(alloc, cap, vsz, alignof(max_align_t));
    ht->states = scl_calloc(alloc, cap, sizeof(scl_hash_slot_state_t), alignof(max_align_t));
    if (!ht->keys || !ht->values || !ht->states) {
        scl_free(alloc, ht->keys);
        scl_free(alloc, ht->values);
        scl_free(alloc, ht->states);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    ht->capacity = cap;
    ht->mask = cap - 1;
    ht->count = 0;
    ht->hash_func = hf;
    ht->eq_func = eq ? eq : scl_hash_eq_mem;
    ht->key_size = ksz;
    ht->value_size = vsz;
    return SCL_OK;
}

void scl_hash_destroy(scl_allocator_t *alloc, scl_hash_t *ht)
{
    if (!ht) return;
    scl_free(alloc, ht->keys);
    scl_free(alloc, ht->values);
    scl_free(alloc, ht->states);
    ht->keys = NULL;
    ht->values = NULL;
    ht->states = NULL;
    ht->capacity = 0;
    ht->count = 0;
}

scl_error_t scl_hash_insert(scl_allocator_t *alloc, scl_hash_t *ht, const void *key, const void *value)
{
    if (!ht || !key || !value) return SCL_ERR_NULL_PTR;

    size_t cap = ht->capacity;
    size_t mask = ht->mask;
    size_t ksz = ht->key_size;
    size_t vsz = ht->value_size;
    scl_hash_func_t hf = ht->hash_func;
    scl_hash_eq_func_t eq = ht->eq_func;

    /* Grow if load > 2/3 */
    if (scl_unlikely(ht->count * 3 >= cap * 2)) {
        scl_error_t err = scl_hash_grow(alloc, ht);
        if (err != SCL_OK) return err;
        cap = ht->capacity;
        mask = ht->mask;
    }

    size_t idx = hf(key, ksz) & mask;
    size_t start = idx;

    /* Linear probing: find existing key or empty slot */
    for (;;) {
        scl_hash_slot_state_t st = ht->states[idx];
        if (st == SCL_HASH_EMPTY || st == SCL_HASH_TOMBSTONE) {
            memcpy(ht->keys + idx * ksz, key, ksz);
            memcpy(ht->values + idx * vsz, value, vsz);
            ht->states[idx] = SCL_HASH_OCCUPIED;
            ht->count++;
            return SCL_OK;
        }
        if (eq(ht->keys + idx * ksz, key, ksz)) {
            memcpy(ht->values + idx * vsz, value, vsz);
            return SCL_OK;
        }
        idx = (idx + 1) & mask;
        if (idx == start) break;
    }

    return SCL_ERR_FULL;
}

scl_error_t scl_hash_get(const scl_hash_t *ht, const void *key, void *out_value)
{
    if (!ht || !key || !out_value) return SCL_ERR_NULL_PTR;

    size_t mask = ht->mask;
    size_t ksz = ht->key_size;
    scl_hash_func_t hf = ht->hash_func;
    scl_hash_eq_func_t eq = ht->eq_func;

    size_t idx = hf(key, ksz) & mask;
    size_t start = idx;

    for (;;) {
        scl_hash_slot_state_t st = ht->states[idx];
        if (st == SCL_HASH_EMPTY) break;
        if (st == SCL_HASH_OCCUPIED && eq(ht->keys + idx * ksz, key, ksz)) {
            memcpy(out_value, ht->values + idx * ht->value_size, ht->value_size);
            return SCL_OK;
        }
        idx = (idx + 1) & mask;
        if (idx == start) break;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_hash_remove(scl_allocator_t *alloc, scl_hash_t *ht, const void *key)
{
    (void)alloc;
    if (!ht || !key) return SCL_ERR_NULL_PTR;

    size_t mask = ht->mask;
    size_t ksz = ht->key_size;
    scl_hash_func_t hf = ht->hash_func;
    scl_hash_eq_func_t eq = ht->eq_func;

    size_t idx = hf(key, ksz) & mask;
    size_t start = idx;

    for (;;) {
        scl_hash_slot_state_t st = ht->states[idx];
        if (st == SCL_HASH_EMPTY) break;
        if (st == SCL_HASH_OCCUPIED && eq(ht->keys + idx * ksz, key, ksz)) {
            ht->states[idx] = SCL_HASH_TOMBSTONE;
            ht->count--;
            return SCL_OK;
        }
        idx = (idx + 1) & mask;
        if (idx == start) break;
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_hash_contains(const scl_hash_t *ht, const void *key)
{
    if (!ht || !key) return false;

    size_t mask = ht->mask;
    size_t ksz = ht->key_size;
    scl_hash_func_t hf = ht->hash_func;
    scl_hash_eq_func_t eq = ht->eq_func;

    size_t idx = hf(key, ksz) & mask;
    size_t start = idx;

    for (;;) {
        scl_hash_slot_state_t st = ht->states[idx];
        if (st == SCL_HASH_EMPTY) break;
        if (st == SCL_HASH_OCCUPIED && eq(ht->keys + idx * ksz, key, ksz))
            return true;
        idx = (idx + 1) & mask;
        if (idx == start) break;
    }
    return false;
}

size_t scl_hash_count(const scl_hash_t *ht)
{
    return ht ? ht->count : 0;
}
