#ifndef SCL_SEARCH_TRIE_SEARCH_H
#define SCL_SEARCH_TRIE_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stdbool.h>

#define SCL_SEARCH_TRIE_ALPHABET_SIZE 256

typedef struct scl_search_trie_node {
    struct scl_search_trie_node *children[SCL_SEARCH_TRIE_ALPHABET_SIZE];
    bool is_end;
} scl_search_trie_node_t;

typedef struct {
    scl_search_trie_node_t *root;
    scl_allocator_t *alloc;
} scl_search_trie_t;

scl_error_t scl_search_trie_init(scl_allocator_t *alloc, scl_search_trie_t **trie) SCL_WARN_UNUSED;
scl_error_t scl_search_trie_insert(scl_search_trie_t *trie, const char *word) SCL_WARN_UNUSED;
bool scl_search_trie_search(const scl_search_trie_t *trie, const char *word);
bool scl_search_trie_starts_with(const scl_search_trie_t *trie, const char *prefix);
scl_error_t scl_search_trie_delete(scl_search_trie_t *trie, const char *word);
void scl_search_trie_destroy(scl_search_trie_t *trie);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
