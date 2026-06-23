#ifndef SCL_HASH_H
#define SCL_HASH_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_hash_func_t)(const void *key, size_t len);
typedef bool   (*scl_hash_eq_func_t)(const void *a, const void *b, size_t key_size);

/* Open-addressing hash table with inline key/value storage.
 * No per-entry allocations — flat arrays, cache-friendly.
 * Uses tombstone markers for deletion.
 */
typedef enum {
    SCL_HASH_EMPTY    = 0,
    SCL_HASH_OCCUPIED = 1,
    SCL_HASH_TOMBSTONE = 2
} scl_hash_slot_state_t;

typedef struct {
    unsigned char *keys;        /* key_size * capacity flat array */
    unsigned char *values;      /* value_size * capacity flat array */
    scl_hash_slot_state_t *states; /* state per slot */
    size_t capacity;
    size_t mask;                /* capacity - 1 (power-of-2) */
    size_t count;
    scl_hash_func_t hash_func;
    scl_hash_eq_func_t eq_func;
    size_t key_size;
    size_t value_size;
} scl_hash_t;

scl_error_t scl_hash_init(scl_allocator_t *alloc, scl_hash_t *ht, size_t key_size, size_t value_size,
                          size_t initial_buckets, scl_hash_func_t hf,
                          scl_hash_eq_func_t eq) SCL_WARN_UNUSED;
void        scl_hash_destroy(scl_allocator_t *alloc, scl_hash_t *ht);
scl_error_t scl_hash_insert(scl_allocator_t *alloc, scl_hash_t *ht, const void *key, const void *value) SCL_WARN_UNUSED;
scl_error_t scl_hash_get(const scl_hash_t *ht, const void *key, void *out_value) SCL_WARN_UNUSED;
scl_error_t scl_hash_remove(scl_allocator_t *alloc, scl_hash_t *ht, const void *key) SCL_WARN_UNUSED;
bool        scl_hash_contains(const scl_hash_t *ht, const void *key);
size_t      scl_hash_count(const scl_hash_t *ht);

size_t scl_hash_djb2(const void *key, size_t len);
bool   scl_hash_eq_mem(const void *a, const void *b, size_t key_size);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
