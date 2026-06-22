#include "scl_bst.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_bst_init(scl_bst_t *tree, size_t element_size, scl_cmp_func_t cmp)
{
    if (!tree || !cmp) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    tree->root = NULL;
    tree->element_size = element_size;
    tree->count = 0;
    tree->cmp = cmp;
    return SCL_OK;
}

static void scl_bst_destroy_node(scl_bst_node_t *node)
{
    if (!node) return;
    scl_bst_destroy_node(node->left);
    scl_bst_destroy_node(node->right);
    free(node->data);
    free(node);
}

void scl_bst_destroy(scl_bst_t *tree)
{
    if (tree) {
        scl_bst_destroy_node(tree->root);
        tree->root = NULL;
        tree->count = 0;
    }
}

static scl_error_t scl_bst_create_node(scl_bst_node_t **out, const void *data, size_t element_size)
{
    scl_bst_node_t *node = malloc(sizeof(scl_bst_node_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;

    node->data = malloc(element_size);
    if (!node->data) {
        free(node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, data, element_size);
    node->left = NULL;
    node->right = NULL;
    *out = node;
    return SCL_OK;
}

scl_error_t scl_bst_insert(scl_bst_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;

    scl_bst_node_t *node;
    scl_error_t err = scl_bst_create_node(&node, element, tree->element_size);
    if (err != SCL_OK) return err;

    if (!tree->root) {
        tree->root = node;
        tree->count = 1;
        return SCL_OK;
    }

    scl_bst_node_t *cur = tree->root;
    while (1) {
        int c = tree->cmp(element, cur->data);
        if (c < 0) {
            if (!cur->left) { cur->left = node; break; }
            cur = cur->left;
        } else if (c > 0) {
            if (!cur->right) { cur->right = node; break; }
            cur = cur->right;
        } else {
            memcpy(cur->data, element, tree->element_size);
            free(node->data);
            free(node);
            return SCL_OK;
        }
    }
    tree->count++;
    return SCL_OK;
}

scl_error_t scl_bst_remove(scl_bst_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;

    scl_bst_node_t *parent = NULL;
    scl_bst_node_t *cur = tree->root;

    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) { parent = cur; cur = cur->left; }
        else if (c > 0) { parent = cur; cur = cur->right; }
        else break;
    }
    if (!cur) return SCL_ERR_NOT_FOUND;

    if (cur->left && cur->right) {
        scl_bst_node_t *succ_parent = cur;
        scl_bst_node_t *succ = cur->right;
        while (succ->left) {
            succ_parent = succ;
            succ = succ->left;
        }
        memcpy(cur->data, succ->data, tree->element_size);
        cur = succ;
        parent = succ_parent;
    }

    scl_bst_node_t *child = cur->left ? cur->left : cur->right;
    if (!parent) {
        tree->root = child;
    } else if (parent->left == cur) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    free(cur->data);
    free(cur);
    tree->count--;
    return SCL_OK;
}

bool scl_bst_contains(const scl_bst_t *tree, const void *key)
{
    if (!tree || !key) return false;
    scl_bst_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else return true;
    }
    return false;
}

scl_error_t scl_bst_find(const scl_bst_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    scl_bst_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else { memcpy(out, cur->data, tree->element_size); return SCL_OK; }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_bst_min(const scl_bst_t *tree, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (!tree->root) return SCL_ERR_EMPTY;
    scl_bst_node_t *cur = tree->root;
    while (cur->left) cur = cur->left;
    memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

scl_error_t scl_bst_max(const scl_bst_t *tree, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (!tree->root) return SCL_ERR_EMPTY;
    scl_bst_node_t *cur = tree->root;
    while (cur->right) cur = cur->right;
    memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

size_t scl_bst_count(const scl_bst_t *tree)
{
    return tree ? tree->count : 0;
}

bool scl_bst_empty(const scl_bst_t *tree)
{
    return tree ? tree->count == 0 : true;
}

scl_error_t scl_bst_inorder(const scl_bst_t *tree, scl_visit_func_t visit, void *ctx)
{
    if (!tree || !visit) return SCL_ERR_NULL_PTR;

    scl_bst_node_t *stack[256];
    int sp = -1;
    scl_bst_node_t *cur = tree->root;

    while (cur || sp >= 0) {
        while (cur) {
            if (sp >= 255) return SCL_ERR_INVALID_STATE;
            stack[++sp] = cur;
            cur = cur->left;
        }
        cur = stack[sp--];
        visit(cur->data, ctx);
        cur = cur->right;
    }
    return SCL_OK;
}
