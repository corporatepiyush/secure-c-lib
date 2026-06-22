#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_trie_search.h"
#include <stdlib.h>
#include <string.h>

static scl_search_trie_node_t *node_create(void)
{
    scl_search_trie_node_t *n = (scl_search_trie_node_t *)calloc(1, sizeof(scl_search_trie_node_t));
    if (n) n->is_end = false;
    return n;
}

scl_error_t scl_search_trie_init(scl_search_trie_t **trie)
{
    if (__builtin_expect(trie == NULL, 0)) return SCL_ERR_NULL_PTR;
    scl_search_trie_t *t = (scl_search_trie_t *)malloc(sizeof(scl_search_trie_t));
    if (__builtin_expect(t == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;
    t->root = node_create();
    if (!t->root) { free(t); return SCL_ERR_OUT_OF_MEMORY; }
    *trie = t;
    return SCL_OK;
}

scl_error_t scl_search_trie_insert(scl_search_trie_t *trie, const char *word)
{
    if (__builtin_expect(trie == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(word == NULL, 0)) return SCL_ERR_NULL_PTR;
    scl_search_trie_node_t *node = trie->root;
    while (*word) {
        unsigned char c = (unsigned char)*word;
        if (!node->children[c]) {
            node->children[c] = node_create();
            if (!node->children[c]) return SCL_ERR_OUT_OF_MEMORY;
        }
        node = node->children[c];
        word++;
    }
    node->is_end = true;
    return SCL_OK;
}

bool scl_search_trie_search(const scl_search_trie_t *trie, const char *word)
{
    if (!trie || !word) return false;
    scl_search_trie_node_t *node = trie->root;
    while (*word) {
        unsigned char c = (unsigned char)*word;
        if (!node->children[c]) return false;
        node = node->children[c];
        word++;
    }
    return node->is_end;
}

bool scl_search_trie_starts_with(const scl_search_trie_t *trie, const char *prefix)
{
    if (!trie || !prefix) return false;
    scl_search_trie_node_t *node = trie->root;
    while (*prefix) {
        unsigned char c = (unsigned char)*prefix;
        if (!node->children[c]) return false;
        node = node->children[c];
        prefix++;
    }
    return true;
}

static bool node_has_children(const scl_search_trie_node_t *node)
{
    for (int i = 0; i < SCL_SEARCH_TRIE_ALPHABET_SIZE; i++)
        if (node->children[i]) return true;
    return false;
}

static bool delete_recursive(scl_search_trie_node_t *node, const char *word)
{
    if (!*word) {
        if (!node->is_end) return false;
        node->is_end = false;
        return !node_has_children(node);
    }
    unsigned char c = (unsigned char)*word;
    if (!node->children[c]) return false;
    bool should_delete = delete_recursive(node->children[c], word + 1);
    if (should_delete) {
        free(node->children[c]);
        node->children[c] = NULL;
        return !node_has_children(node) && !node->is_end;
    }
    return false;
}

scl_error_t scl_search_trie_delete(scl_search_trie_t *trie, const char *word)
{
    if (__builtin_expect(trie == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(word == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (!trie->root) return SCL_ERR_NOT_FOUND;
    if (!scl_search_trie_search(trie, word)) return SCL_ERR_NOT_FOUND;
    delete_recursive(trie->root, word);
    return SCL_OK;
}

static void destroy_node(scl_search_trie_node_t *node)
{
    if (!node) return;
    for (int i = 0; i < SCL_SEARCH_TRIE_ALPHABET_SIZE; i++)
        destroy_node(node->children[i]);
    free(node);
}

void scl_search_trie_destroy(scl_search_trie_t *trie)
{
    if (!trie) return;
    destroy_node(trie->root);
    free(trie);
}
