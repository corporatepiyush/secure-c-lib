#include "scl_trie.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_trie_init(scl_allocator_t *alloc, scl_trie_t *trie, size_t value_size)
{
    if (scl_unlikely(!trie)) return SCL_ERR_NULL_PTR;

    trie->root = scl_calloc(alloc, 1, sizeof(scl_trie_node_t), alignof(max_align_t));
    if (scl_unlikely(!trie->root)) return SCL_ERR_OUT_OF_MEMORY;

    trie->value_size = value_size;
    trie->count = 0;
    return SCL_OK;
}

void scl_trie_destroy(scl_allocator_t *alloc, scl_trie_t *trie)
{
    if (scl_unlikely(!trie || !trie->root)) return;

    scl_trie_node_t *stack1[4096];
    int sp1 = 0;
    stack1[sp1++] = trie->root;

    scl_trie_node_t *stack2[4096];
    int sp2 = 0;

    while (sp1 > 0) {
        scl_trie_node_t *node = stack1[--sp1];
        stack2[sp2++] = node;
        for (int i = 0; i < SCL_TRIE_ALPHABET; i++) {
            if (node->children[i])
                stack1[sp1++] = node->children[i];
        }
    }

    while (sp2 > 0) {
        scl_trie_node_t *node = stack2[--sp2];
        scl_free(alloc, node->value);
        scl_free(alloc, node);
    }

    trie->root = NULL;
    trie->count = 0;
}

scl_error_t scl_trie_insert(scl_allocator_t *alloc, scl_trie_t *trie, const unsigned char *key, size_t key_len,
                            const void *value)
{
    if (scl_unlikely(!trie || !key || !value)) return SCL_ERR_NULL_PTR;

    scl_trie_node_t *node = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (scl_unlikely(!node->children[c])) {
            node->children[c] = scl_calloc(alloc, 1, sizeof(scl_trie_node_t), alignof(max_align_t));
            if (scl_unlikely(!node->children[c])) return SCL_ERR_OUT_OF_MEMORY;
        }
        node = node->children[c];
    }

    if (!node->terminal) {
        node->terminal = true;
        trie->count++;
    }

    if (scl_unlikely(!node->value)) {
        node->value = scl_alloc(alloc, trie->value_size, alignof(max_align_t));
        if (scl_unlikely(!node->value)) return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(node->value, value, trie->value_size);
    return SCL_OK;
}

scl_error_t scl_trie_get(const scl_trie_t *trie, const unsigned char *key, size_t key_len,
                         void *out_value)
{
    if (scl_unlikely(!trie || !key || !out_value)) return SCL_ERR_NULL_PTR;

    scl_trie_node_t *node = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (scl_unlikely(!node->children[c])) return SCL_ERR_NOT_FOUND;
        node = node->children[c];
    }

    if (scl_unlikely(!node->terminal || !node->value)) return SCL_ERR_NOT_FOUND;
    scl_memcpy(out_value, node->value, trie->value_size);
    return SCL_OK;
}

bool scl_trie_contains(const scl_trie_t *trie, const unsigned char *key, size_t key_len)
{
    if (scl_unlikely(!trie || !key)) return false;
    scl_trie_node_t *node = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (scl_unlikely(!node->children[c])) return false;
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

scl_error_t scl_trie_remove(scl_allocator_t *alloc, scl_trie_t *trie, const unsigned char  *SCL_RESTRICT key, size_t key_len)
{
    if (scl_unlikely(!trie || !key)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!scl_trie_contains(trie, key, key_len))) return SCL_ERR_NOT_FOUND;

    scl_trie_node_t *path[1024];
    int path_len = 0;
    scl_trie_node_t *node = trie->root;
    path[path_len++] = node;

    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        node = node->children[c];
        path[path_len++] = node;
    }

    node->terminal = false;
    scl_free(alloc, node->value);
    node->value = NULL;

    for (int i = path_len - 1; i > 0; i--) {
        scl_trie_node_t *n = path[i];
        if (n->terminal || scl_trie_has_children(n)) break;

        unsigned char c = key[i - 1];
        scl_trie_node_t *parent = path[i - 1];
        parent->children[c] = NULL;
        scl_free(alloc, n);
    }

    trie->count--;
    return SCL_OK;
}

size_t scl_trie_count(const scl_trie_t *trie)
{
    return trie ? trie->count : 0;
}
