#include "scl_search_hash_search.h"
#include <string.h>

static size_t hash_str(const char *s, size_t cap)
{
    size_t h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + (size_t)c;
    return h % cap;
}

static char *str_dup(scl_allocator_t *alloc, const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = (char *)scl_alloc(alloc, len, alignof(max_align_t));
    if (!copy) return NULL;
    memcpy(copy, s, len);
    return copy;
}

scl_error_t scl_search_ht_init(scl_allocator_t *alloc, scl_search_ht_t **ht, size_t capacity)
{
    if (__builtin_expect(ht == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(capacity == 0, 0)) return SCL_ERR_INVALID_ARG;
    scl_search_ht_t *t = (scl_search_ht_t *)scl_alloc(alloc, sizeof(scl_search_ht_t), alignof(max_align_t));
    if (!t) return SCL_ERR_OUT_OF_MEMORY;
    t->entries = (scl_search_ht_entry_t *)scl_calloc(alloc, capacity, sizeof(scl_search_ht_entry_t), alignof(max_align_t));
    if (!t->entries) { scl_free(alloc, t); return SCL_ERR_OUT_OF_MEMORY; }
    t->capacity = capacity;
    t->count = 0;
    t->alloc = alloc;
    *ht = t;
    return SCL_OK;
}

scl_error_t scl_search_ht_insert(scl_search_ht_t *ht, const char *key, void *value)
{
    if (__builtin_expect(ht == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (ht->count >= ht->capacity / 2) return SCL_ERR_FULL;

    size_t idx = hash_str(key, ht->capacity);
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t probe = (idx + i) % ht->capacity;
        if (!ht->entries[probe].occupied || ht->entries[probe].deleted) {
            ht->entries[probe].key = str_dup(ht->alloc, key);
            if (!ht->entries[probe].key) return SCL_ERR_OUT_OF_MEMORY;
            ht->entries[probe].value = value;
            ht->entries[probe].occupied = true;
            ht->entries[probe].deleted = false;
            ht->count++;
            return SCL_OK;
        }
        if (strcmp(ht->entries[probe].key, key) == 0) {
            ht->entries[probe].value = value;
            return SCL_OK;
        }
    }
    return SCL_ERR_FULL;
}

scl_error_t scl_search_ht_search(const scl_search_ht_t *ht, const char *key, void **out_value)
{
    if (__builtin_expect(ht == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_value == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(ht->count == 0, 0)) return SCL_ERR_EMPTY;

    size_t idx = hash_str(key, ht->capacity);
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t probe = (idx + i) % ht->capacity;
        if (!ht->entries[probe].occupied) return SCL_ERR_NOT_FOUND;
        if (!ht->entries[probe].deleted && strcmp(ht->entries[probe].key, key) == 0) {
            *out_value = ht->entries[probe].value;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_search_ht_delete(scl_search_ht_t *ht, const char *key)
{
    if (__builtin_expect(ht == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(ht->count == 0, 0)) return SCL_ERR_EMPTY;

    size_t idx = hash_str(key, ht->capacity);
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t probe = (idx + i) % ht->capacity;
        if (!ht->entries[probe].occupied) return SCL_ERR_NOT_FOUND;
        if (!ht->entries[probe].deleted && strcmp(ht->entries[probe].key, key) == 0) {
            ht->entries[probe].deleted = true;
            scl_free(ht->alloc, ht->entries[probe].key);
            ht->entries[probe].key = NULL;
            ht->count--;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}

void scl_search_ht_destroy(scl_search_ht_t *ht)
{
    if (!ht) return;
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->entries[i].occupied && !ht->entries[i].deleted)
            scl_free(ht->alloc, ht->entries[i].key);
    }
    scl_free(ht->alloc, ht->entries);
    scl_free(ht->alloc, ht);
}
