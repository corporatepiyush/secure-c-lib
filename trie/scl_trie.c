#include "scl_trie.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_trie_init(scl_trie_t *trie, size_t value_size)
{
    if (!trie) return SCL_ERR_NULL_PTR;

    trie->root = calloc(1, sizeof(scl_trie_node_t));
    if (!trie->root) return SCL_ERR_OUT_OF_MEMORY;

    trie->value_size = value_size;
    trie->count = 0;
    return SCL_OK;
}

static void scl_trie_destroy_node(scl_trie_node_t *node)
{
    if (!node) return;
    for (int i = 0; i < SCL_TRIE_ALPHABET; i++)
        scl_trie_destroy_node(node->children[i]);
    free(node->value);
    free(node);
}

void scl_trie_destroy(scl_trie_t *trie)
{
    if (trie) {
        scl_trie_destroy_node(trie->root);
        trie->root = NULL;
        trie->count = 0;
    }
}

scl_error_t scl_trie_insert(scl_trie_t *trie, const unsigned char *key, size_t key_len,
                            const void *value)
{
    if (!trie || !key || !value) return SCL_ERR_NULL_PTR;

    scl_trie_node_t *node = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (!node->children[c]) {
            node->children[c] = calloc(1, sizeof(scl_trie_node_t));
            if (!node->children[c]) return SCL_ERR_OUT_OF_MEMORY;
        }
        node = node->children[c];
    }

    if (!node->terminal) {
        node->terminal = true;
        trie->count++;
    }

    if (!node->value) {
        node->value = malloc(trie->value_size);
        if (!node->value) return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->value, value, trie->value_size);
    return SCL_OK;
}

scl_error_t scl_trie_get(const scl_trie_t *trie, const unsigned char *key, size_t key_len,
                         void *out_value)
{
    if (!trie || !key || !out_value) return SCL_ERR_NULL_PTR;

    scl_trie_node_t *node = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (!node->children[c]) return SCL_ERR_NOT_FOUND;
        node = node->children[c];
    }

    if (!node->terminal || !node->value) return SCL_ERR_NOT_FOUND;
    memcpy(out_value, node->value, trie->value_size);
    return SCL_OK;
}

bool scl_trie_contains(const scl_trie_t *trie, const unsigned char *key, size_t key_len)
{
    if (!trie || !key) return false;
    scl_trie_node_t *node = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (!node->children[c]) return false;
        node = node->children[c];
    }
    return node->terminal;
}

static bool scl_trie_has_children(const scl_trie_node_t *node)
{
    for (int i = 0; i < SCL_TRIE_ALPHABET; i++)
        if (node->children[i]) return true;
    return false;
}

static bool scl_trie_remove_node(scl_trie_node_t *node, const unsigned char *key,
                                  size_t key_len, size_t depth)
{
    if (!node) return false;
    if (depth == key_len) {
        if (!node->terminal) return false;
        node->terminal = false;
        free(node->value);
        node->value = NULL;
        return !scl_trie_has_children(node);
    }

    unsigned char c = key[depth];
    if (!node->children[c]) return false;

    bool should_delete = scl_trie_remove_node(node->children[c], key, key_len, depth + 1);
    if (should_delete) {
        free(node->children[c]);
        node->children[c] = NULL;
        return !node->terminal && !scl_trie_has_children(node);
    }
    return false;
}

scl_error_t scl_trie_remove(scl_trie_t *trie, const unsigned char *key, size_t key_len)
{
    if (!trie || !key) return SCL_ERR_NULL_PTR;
    if (!scl_trie_contains(trie, key, key_len)) return SCL_ERR_NOT_FOUND;

    scl_trie_remove_node(trie->root, key, key_len, 0);
    trie->count--;
    return SCL_OK;
}

size_t scl_trie_count(const scl_trie_t *trie)
{
    return trie ? trie->count : 0;
}
