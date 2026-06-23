#ifndef SCL_CONCURRENT_TRIE_H
#define SCL_CONCURRENT_TRIE_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_TRIE_ALPHABET 256

typedef struct scl_concurrent_trie_node {
    struct scl_concurrent_trie_node *children[SCL_TRIE_ALPHABET];
    void *value;
    bool terminal;
    scl_spinlock_t lock;
} scl_concurrent_trie_node_t;

typedef struct {
    scl_concurrent_trie_node_t *root;
    size_t value_size;
    atomic_size_t count;
} scl_concurrent_trie_t;

scl_error_t scl_ctrie_init(scl_allocator_t *alloc, scl_concurrent_trie_t *trie, size_t value_size) SCL_WARN_UNUSED;
void        scl_ctrie_destroy(scl_allocator_t *alloc, scl_concurrent_trie_t *trie);
scl_error_t scl_ctrie_insert(scl_allocator_t *alloc, scl_concurrent_trie_t *trie, const unsigned char *key,
                            size_t key_len, const void *value) SCL_WARN_UNUSED;
scl_error_t scl_ctrie_get(const scl_concurrent_trie_t *trie, const unsigned char *key,
                         size_t key_len, void *out_value) SCL_WARN_UNUSED;
bool        scl_ctrie_contains(const scl_concurrent_trie_t *trie, const unsigned char *key,
                              size_t key_len);
scl_error_t scl_ctrie_remove(scl_allocator_t *alloc, scl_concurrent_trie_t *trie, const unsigned char *key,
                            size_t key_len) SCL_WARN_UNUSED;
size_t      scl_ctrie_count(const scl_concurrent_trie_t *trie);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
