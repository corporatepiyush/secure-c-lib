#ifndef SCL_TRIE_H
#define SCL_TRIE_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_TRIE_ALPHABET 256

typedef struct scl_trie_node {
    struct scl_trie_node *children[SCL_TRIE_ALPHABET];
    void *value;
    bool terminal;
} scl_trie_node_t;

typedef struct {
    scl_trie_node_t *root;
    size_t value_size;
    size_t count;
} scl_trie_t;

scl_error_t scl_trie_init(scl_allocator_t *SCL_RESTRICT alloc, scl_trie_t *SCL_RESTRICT trie, size_t value_size) SCL_WARN_UNUSED;
void        scl_trie_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_trie_t *SCL_RESTRICT trie);
scl_error_t scl_trie_insert(scl_allocator_t *alloc, scl_trie_t *trie, const unsigned char *key, size_t key_len,
                            const void *value) SCL_WARN_UNUSED;
scl_error_t scl_trie_get(const scl_trie_t *trie, const unsigned char *key, size_t key_len,
                         void *out_value) SCL_WARN_UNUSED;
SCL_PURE bool        scl_trie_contains(const scl_trie_t *SCL_RESTRICT trie, const unsigned char *SCL_RESTRICT key, size_t key_len);
scl_error_t scl_trie_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_trie_t *SCL_RESTRICT trie, const unsigned char *SCL_RESTRICT key, size_t key_len) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_trie_count(const scl_trie_t *SCL_RESTRICT trie);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
