#ifndef SCL_SEARCH_HASH_SEARCH_H
#define SCL_SEARCH_HASH_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef struct {
    char *key;
    void *value;
    bool occupied;
    bool deleted;
} scl_search_ht_entry_t;

typedef struct {
    scl_search_ht_entry_t *entries;
    size_t capacity;
    size_t count;
    scl_allocator_t *alloc;
} scl_search_ht_t;

scl_error_t scl_search_ht_init(scl_allocator_t *alloc, scl_search_ht_t **ht, size_t capacity) SCL_WARN_UNUSED;
scl_error_t scl_search_ht_insert(scl_search_ht_t *ht, const char *key, void *value) SCL_WARN_UNUSED;
scl_error_t scl_search_ht_search(const scl_search_ht_t *ht, const char *key, void **out_value) SCL_WARN_UNUSED;
scl_error_t scl_search_ht_delete(scl_search_ht_t *ht, const char *key);
void scl_search_ht_destroy(scl_search_ht_t *ht);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
