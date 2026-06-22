#include "scl_btree.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_btree_node_t *scl_btree_create_node(bool leaf, int t, size_t key_size, size_t value_size)
{
    scl_btree_node_t *node = malloc(sizeof(scl_btree_node_t));
    if (!node) return NULL;

    size_t max_keys = 2 * t - 1;
    node->keys = calloc(max_keys, key_size);
    node->values = calloc(max_keys, value_size);
    node->children = calloc(2 * t, sizeof(scl_btree_node_t *));
    if (!node->keys || !node->values || !node->children) {
        free(node->keys);
        free(node->values);
        free(node->children);
        free(node);
        return NULL;
    }
    node->count = 0;
    node->leaf = leaf;
    return node;
}

static void scl_btree_destroy_node(scl_btree_node_t *node, int t)
{
    if (!node) return;
    if (!node->leaf) {
        for (size_t i = 0; i <= node->count; i++)
            scl_btree_destroy_node(node->children[i], t);
    }
    free(node->keys);
    free(node->values);
    free(node->children);
    free(node);
}

scl_error_t scl_btree_init(scl_btree_t *tree, size_t key_size, size_t value_size,
                           int degree, scl_cmp_func_t cmp)
{
    if (!tree || !cmp) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || degree < 2)
        return SCL_ERR_INVALID_ARG;

    tree->root = scl_btree_create_node(true, degree, key_size, value_size);
    if (!tree->root) return SCL_ERR_OUT_OF_MEMORY;

    tree->element_size = key_size;
    tree->value_size = value_size;
    tree->count = 0;
    tree->cmp = cmp;
    tree->t = degree;
    return SCL_OK;
}

void scl_btree_destroy(scl_btree_t *tree)
{
    if (tree) {
        scl_btree_destroy_node(tree->root, tree->t);
        tree->root = NULL;
        tree->count = 0;
    }
}

static void scl_btree_split_child(scl_btree_node_t *parent, size_t i, int t,
                                   size_t key_size, size_t value_size)
{
    scl_btree_node_t *child = parent->children[i];
    scl_btree_node_t *new_child = scl_btree_create_node(child->leaf, t, key_size, value_size);

    new_child->count = t - 1;
    for (int j = 0; j < t - 1; j++) {
        memcpy((unsigned char *)new_child->keys + j * key_size,
               (unsigned char *)child->keys + (j + t) * key_size, key_size);
        memcpy((unsigned char *)new_child->values + j * value_size,
               (unsigned char *)child->values + (j + t) * value_size, value_size);
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
        memcpy((unsigned char *)parent->keys + (j + 1) * key_size,
               (unsigned char *)parent->keys + j * key_size, key_size);
        memcpy((unsigned char *)parent->values + (j + 1) * value_size,
               (unsigned char *)parent->values + j * value_size, value_size);
    }
    memcpy((unsigned char *)parent->keys + i * key_size,
           (unsigned char *)child->keys + (t - 1) * key_size, key_size);
    memcpy((unsigned char *)parent->values + i * value_size,
           (unsigned char *)child->values + (t - 1) * value_size, value_size);
    parent->count++;
}

static void scl_btree_insert_nonfull(scl_btree_node_t *node, const void *key,
                                      const void *value, int t,
                                      size_t key_size, size_t value_size,
                                      scl_cmp_func_t cmp)
{
    int i = (int)node->count - 1;
    if (node->leaf) {
        while (i >= 0 && cmp(key, (unsigned char *)node->keys + i * key_size) < 0) {
            memcpy((unsigned char *)node->keys + (i + 1) * key_size,
                   (unsigned char *)node->keys + i * key_size, key_size);
            memcpy((unsigned char *)node->values + (i + 1) * value_size,
                   (unsigned char *)node->values + i * value_size, value_size);
            i--;
        }
        i++;
        memcpy((unsigned char *)node->keys + i * key_size, key, key_size);
        memcpy((unsigned char *)node->values + i * value_size, value, value_size);
        node->count++;
    } else {
        while (i >= 0 && cmp(key, (unsigned char *)node->keys + i * key_size) < 0)
            i--;
        i++;
        if (node->children[i]->count == (size_t)(2 * t - 1)) {
            scl_btree_split_child(node, i, t, key_size, value_size);
            if (cmp(key, (unsigned char *)node->keys + i * key_size) > 0)
                i++;
        }
        scl_btree_insert_nonfull(node->children[i], key, value, t,
                                  key_size, value_size, cmp);
    }
}

scl_error_t scl_btree_insert(scl_btree_t *tree, const void *key, const void *value)
{
    if (!tree || !key || !value) return SCL_ERR_NULL_PTR;

    if (tree->root->count == (size_t)(2 * tree->t - 1)) {
        scl_btree_node_t *new_root = scl_btree_create_node(false, tree->t,
                                                           tree->element_size,
                                                           tree->value_size);
        if (!new_root) return SCL_ERR_OUT_OF_MEMORY;
        new_root->children[0] = tree->root;
        tree->root = new_root;
        scl_btree_split_child(new_root, 0, tree->t, tree->element_size, tree->value_size);
    }
    scl_btree_insert_nonfull(tree->root, key, value, tree->t,
                              tree->element_size, tree->value_size, tree->cmp);
    tree->count++;
    return SCL_OK;
}

scl_error_t scl_btree_get(const scl_btree_t *tree, const void *key, void *out_value)
{
    if (!tree || !key || !out_value) return SCL_ERR_NULL_PTR;

    scl_btree_node_t *node = tree->root;
    while (node) {
        int i = 0;
        while (i < (int)node->count && tree->cmp(key,
                   (unsigned char *)node->keys + i * tree->element_size) > 0)
            i++;
        if (i < (int)node->count && tree->cmp(key,
                   (unsigned char *)node->keys + i * tree->element_size) == 0) {
            memcpy(out_value, (unsigned char *)node->values + i * tree->value_size,
                   tree->value_size);
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
        while (i < (int)node->count && tree->cmp(key,
                   (unsigned char *)node->keys + i * tree->element_size) > 0)
            i++;
        if (i < (int)node->count && tree->cmp(key,
                   (unsigned char *)node->keys + i * tree->element_size) == 0)
            return true;
        if (node->leaf) break;
        node = node->children[i];
    }
    return false;
}

size_t scl_btree_count(const scl_btree_t *tree) { return tree ? tree->count : 0; }
bool scl_btree_empty(const scl_btree_t *tree) { return tree ? tree->count == 0 : true; }
