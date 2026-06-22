#include "scl_btree.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_btree_node_t *scl_btree_create_node(scl_allocator_t *alloc, bool leaf, int t, size_t key_size, size_t value_size)
{
    scl_btree_node_t *node = scl_alloc(alloc, sizeof(scl_btree_node_t), alignof(max_align_t));
    if (!node) return NULL;

    size_t max_keys = 2 * t - 1;
    node->keys = scl_calloc(alloc, max_keys, sizeof(void *), alignof(max_align_t));
    node->values = scl_calloc(alloc, max_keys, sizeof(void *), alignof(max_align_t));
    node->children = scl_calloc(alloc, 2 * t, sizeof(scl_btree_node_t *), alignof(max_align_t));
    if (!node->keys || !node->values || !node->children) {
        scl_free(alloc, node->keys);
        scl_free(alloc, node->values);
        scl_free(alloc, node->children);
        scl_free(alloc, node);
        return NULL;
    }
    for (size_t i = 0; i < max_keys; i++) {
        node->keys[i] = scl_alloc(alloc, key_size, alignof(max_align_t));
        node->values[i] = scl_alloc(alloc, value_size, alignof(max_align_t));
        if (!node->keys[i] || !node->values[i]) {
            for (size_t j = 0; j <= i; j++) {
                scl_free(alloc, node->keys[j]);
                scl_free(alloc, node->values[j]);
            }
            scl_free(alloc, node->keys);
            scl_free(alloc, node->values);
            scl_free(alloc, node->children);
            scl_free(alloc, node);
            return NULL;
        }
    }
    node->count = 0;
    node->leaf = leaf;
    return node;
}

void scl_btree_destroy(scl_allocator_t *alloc, scl_btree_t *tree)
{
    if (!tree || !tree->root) return;

    scl_btree_node_t *stack[256];
    int sp = 0;
    stack[sp++] = tree->root;
    scl_btree_node_t *stack2[256];
    int sp2 = 0;

    while (sp > 0) {
        scl_btree_node_t *node = stack[--sp];
        stack2[sp2++] = node;
        if (!node->leaf) {
            for (size_t i = 0; i <= node->count; i++)
                if (node->children[i])
                    stack[sp++] = node->children[i];
        }
    }

    while (sp2 > 0) {
        scl_btree_node_t *node = stack2[--sp2];
        size_t max_keys = 2 * tree->t - 1;
        for (size_t i = 0; i < max_keys; i++) {
            scl_free(alloc, node->keys[i]);
            scl_free(alloc, node->values[i]);
        }
        scl_free(alloc, node->keys);
        scl_free(alloc, node->values);
        scl_free(alloc, node->children);
        scl_free(alloc, node);
    }

    tree->root = NULL;
    tree->count = 0;
}

scl_error_t scl_btree_init(scl_allocator_t *alloc, scl_btree_t *tree, size_t key_size, size_t value_size,
                           int degree, scl_cmp_func_t cmp)
{
    if (!tree || !cmp) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || degree < 2)
        return SCL_ERR_INVALID_ARG;

    tree->root = scl_btree_create_node(alloc, true, degree, key_size, value_size);
    if (!tree->root) return SCL_ERR_OUT_OF_MEMORY;

    tree->element_size = key_size;
    tree->value_size = value_size;
    tree->count = 0;
    tree->cmp = cmp;
    tree->t = degree;
    return SCL_OK;
}

static void scl_btree_split_child(scl_allocator_t *alloc, scl_btree_node_t *parent, size_t i, int t,
                                   size_t key_size, size_t value_size)
{
    scl_btree_node_t *child = parent->children[i];
    scl_btree_node_t *new_child = scl_btree_create_node(alloc, child->leaf, t, key_size, value_size);

    new_child->count = t - 1;
    for (int j = 0; j < t - 1; j++) {
        memcpy(new_child->keys[j], child->keys[j + t], key_size);
        memcpy(new_child->values[j], child->values[j + t], value_size);
    }

    if (!child->leaf) {
        for (int j = 0; j < t; j++)
            new_child->children[j] = child->children[j + t];
    }

    child->count = t - 1;

    for (int j = (int)parent->count; j >= (int)i + 1; j--)
        parent->children[j + 1] = parent->children[j];
    parent->children[i + 1] = new_child;

    for (int j = (int)parent->count - 1; j >= (int)i; j--) {
        memcpy(parent->keys[j + 1], parent->keys[j], key_size);
        memcpy(parent->values[j + 1], parent->values[j], value_size);
    }
    memcpy(parent->keys[i], child->keys[t - 1], key_size);
    memcpy(parent->values[i], child->values[t - 1], value_size);
    parent->count++;
}

scl_error_t scl_btree_insert(scl_allocator_t *alloc, scl_btree_t *tree, const void *key, const void *value)
{
    if (!tree || !key || !value) return SCL_ERR_NULL_PTR;

    if (tree->root->count == (size_t)(2 * tree->t - 1)) {
        scl_btree_node_t *new_root = scl_btree_create_node(alloc, false, tree->t,
                                                           tree->element_size,
                                                           tree->value_size);
        if (!new_root) return SCL_ERR_OUT_OF_MEMORY;
        new_root->children[0] = tree->root;
        tree->root = new_root;
        scl_btree_split_child(alloc, new_root, 0, tree->t, tree->element_size, tree->value_size);
    }

    scl_btree_node_t *node = tree->root;
    while (1) {
        int i = (int)node->count - 1;
        if (node->leaf) {
            while (i >= 0 && tree->cmp(key, node->keys[i]) < 0) {
                memcpy(node->keys[i + 1], node->keys[i], tree->element_size);
                memcpy(node->values[i + 1], node->values[i], tree->value_size);
                i--;
            }
            i++;
            if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
                memcpy(node->values[i], value, tree->value_size);
                return SCL_OK;
            }
            memcpy(node->keys[i], key, tree->element_size);
            memcpy(node->values[i], value, tree->value_size);
            node->count++;
            break;
        } else {
            while (i >= 0 && tree->cmp(key, node->keys[i]) < 0)
                i--;
            i++;
            if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
                memcpy(node->values[i], value, tree->value_size);
                return SCL_OK;
            }
            if (node->children[i]->count == (size_t)(2 * tree->t - 1)) {
                scl_btree_split_child(alloc, node, i, tree->t, tree->element_size, tree->value_size);
                if (tree->cmp(key, node->keys[i]) > 0)
                    i++;
            }
            node = node->children[i];
        }
    }

    tree->count++;
    return SCL_OK;
}

scl_error_t scl_btree_get(const scl_btree_t *tree, const void *key, void *out_value)
{
    if (!tree || !key || !out_value) return SCL_ERR_NULL_PTR;

    scl_btree_node_t *node = tree->root;
    while (node) {
        int i = 0;
        while (i < (int)node->count && tree->cmp(key, node->keys[i]) > 0)
            i++;
        if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
            memcpy(out_value, node->values[i], tree->value_size);
            return SCL_OK;
        }
        if (node->leaf) break;
        node = node->children[i];
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_btree_contains(const scl_btree_t *tree, const void *key)
{
    if (!tree || !key) return false;
    scl_btree_node_t *node = tree->root;
    while (node) {
        int i = 0;
        while (i < (int)node->count && tree->cmp(key, node->keys[i]) > 0)
            i++;
        if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0)
            return true;
        if (node->leaf) break;
        node = node->children[i];
    }
    return false;
}

scl_error_t scl_btree_remove(scl_allocator_t *alloc, scl_btree_t *tree, const void *key)
{
    (void)alloc;
    (void)tree;
    (void)key;
    return SCL_ERR_NOT_IMPLEMENTED;
}

size_t scl_btree_count(const scl_btree_t *tree) { return tree ? tree->count : 0; }
bool scl_btree_empty(const scl_btree_t *tree) { return tree ? tree->count == 0 : true; }
