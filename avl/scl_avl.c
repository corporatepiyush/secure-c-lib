#include "scl_avl.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline int scl_avl_height(const scl_avl_node_t *n)
{
    return n ? n->height : -1;
}

static inline int scl_avl_max2(int a, int b)
{
    return a > b ? a : b;
}

static inline void scl_avl_update_height(scl_avl_node_t *n)
{
    n->height = 1 + scl_avl_max2(scl_avl_height(n->left), scl_avl_height(n->right));
}

static inline int scl_avl_balance(const scl_avl_node_t *n)
{
    return n ? scl_avl_height(n->left) - scl_avl_height(n->right) : 0;
}

static scl_avl_node_t *scl_avl_rotate_right(scl_avl_node_t *y)
{
    scl_avl_node_t *x = y->left;
    scl_avl_node_t *t2 = x->right;
    x->right = y;
    y->left = t2;
    scl_avl_update_height(y);
    scl_avl_update_height(x);
    return x;
}

static scl_avl_node_t *scl_avl_rotate_left(scl_avl_node_t *x)
{
    scl_avl_node_t *y = x->right;
    scl_avl_node_t *t2 = y->left;
    y->left = x;
    x->right = t2;
    scl_avl_update_height(x);
    scl_avl_update_height(y);
    return y;
}

scl_error_t scl_avl_init(scl_avl_t *tree, size_t element_size, scl_cmp_func_t cmp)
{
    if (!tree || !cmp) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    tree->root = NULL;
    tree->element_size = element_size;
    tree->count = 0;
    tree->cmp = cmp;
    return SCL_OK;
}

static void scl_avl_destroy_node(scl_avl_node_t *n)
{
    if (!n) return;
    scl_avl_destroy_node(n->left);
    scl_avl_destroy_node(n->right);
    free(n->data);
    free(n);
}

void scl_avl_destroy(scl_avl_t *tree)
{
    if (tree) {
        scl_avl_destroy_node(tree->root);
        tree->root = NULL;
        tree->count = 0;
    }
}

static scl_avl_node_t *scl_avl_create_node(const void *data, size_t element_size)
{
    scl_avl_node_t *n = malloc(sizeof(scl_avl_node_t));
    if (!n) return NULL;
    n->data = malloc(element_size);
    if (!n->data) { free(n); return NULL; }
    memcpy(n->data, data, element_size);
    n->left = NULL;
    n->right = NULL;
    n->height = 0;
    return n;
}

static scl_avl_node_t *scl_avl_insert_node(scl_avl_t *tree, scl_avl_node_t *node,
                                            const void *data, bool *created)
{
    if (!node) {
        *created = true;
        return scl_avl_create_node(data, tree->element_size);
    }

    int c = tree->cmp(data, node->data);
    if (c < 0)
        node->left = scl_avl_insert_node(tree, node->left, data, created);
    else if (c > 0)
        node->right = scl_avl_insert_node(tree, node->right, data, created);
    else {
        memcpy(node->data, data, tree->element_size);
        *created = false;
        return node;
    }

    scl_avl_update_height(node);
    int bal = scl_avl_balance(node);

    if (bal > 1 && tree->cmp(data, node->left->data) < 0)
        return scl_avl_rotate_right(node);
    if (bal < -1 && tree->cmp(data, node->right->data) > 0)
        return scl_avl_rotate_left(node);
    if (bal > 1 && tree->cmp(data, node->left->data) > 0) {
        node->left = scl_avl_rotate_left(node->left);
        return scl_avl_rotate_right(node);
    }
    if (bal < -1 && tree->cmp(data, node->right->data) < 0) {
        node->right = scl_avl_rotate_right(node->right);
        return scl_avl_rotate_left(node);
    }

    return node;
}

scl_error_t scl_avl_insert(scl_avl_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;
    bool created = false;
    tree->root = scl_avl_insert_node(tree, tree->root, element, &created);
    if (!tree->root) return SCL_ERR_OUT_OF_MEMORY;
    if (created) tree->count++;
    return SCL_OK;
}

static scl_avl_node_t *scl_avl_min_node(scl_avl_node_t *n)
{
    while (n && n->left) n = n->left;
    return n;
}

static scl_avl_node_t *scl_avl_remove_node(scl_avl_t *tree, scl_avl_node_t *node,
                                            const void *key, bool *removed)
{
    if (!node) return NULL;

    int c = tree->cmp(key, node->data);
    if (c < 0)
        node->left = scl_avl_remove_node(tree, node->left, key, removed);
    else if (c > 0)
        node->right = scl_avl_remove_node(tree, node->right, key, removed);
    else {
        *removed = true;
        if (!node->left || !node->right) {
            scl_avl_node_t *child = node->left ? node->left : node->right;
            free(node->data);
            free(node);
            return child;
        } else {
            scl_avl_node_t *succ = scl_avl_min_node(node->right);
            memcpy(node->data, succ->data, tree->element_size);
            node->right = scl_avl_remove_node(tree, node->right, succ->data, &(bool){false});
        }
    }

    if (!node) return NULL;
    scl_avl_update_height(node);
    int bal = scl_avl_balance(node);

    if (bal > 1 && scl_avl_balance(node->left) >= 0)
        return scl_avl_rotate_right(node);
    if (bal > 1 && scl_avl_balance(node->left) < 0) {
        node->left = scl_avl_rotate_left(node->left);
        return scl_avl_rotate_right(node);
    }
    if (bal < -1 && scl_avl_balance(node->right) <= 0)
        return scl_avl_rotate_left(node);
    if (bal < -1 && scl_avl_balance(node->right) > 0) {
        node->right = scl_avl_rotate_right(node->right);
        return scl_avl_rotate_left(node);
    }

    return node;
}

scl_error_t scl_avl_remove(scl_avl_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;
    bool removed = false;
    tree->root = scl_avl_remove_node(tree, tree->root, key, &removed);
    if (!removed) return SCL_ERR_NOT_FOUND;
    tree->count--;
    return SCL_OK;
}

bool scl_avl_contains(const scl_avl_t *tree, const void *key)
{
    if (!tree || !key) return false;
    scl_avl_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else return true;
    }
    return false;
}

scl_error_t scl_avl_find(const scl_avl_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    scl_avl_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else { memcpy(out, cur->data, tree->element_size); return SCL_OK; }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_avl_min(const scl_avl_t *tree, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (!tree->root) return SCL_ERR_EMPTY;
    scl_avl_node_t *cur = tree->root;
    while (cur->left) cur = cur->left;
    memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

scl_error_t scl_avl_max(const scl_avl_t *tree, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (!tree->root) return SCL_ERR_EMPTY;
    scl_avl_node_t *cur = tree->root;
    while (cur->right) cur = cur->right;
    memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

size_t scl_avl_count(const scl_avl_t *tree) { return tree ? tree->count : 0; }
bool scl_avl_empty(const scl_avl_t *tree) { return tree ? tree->count == 0 : true; }

scl_error_t scl_avl_inorder(const scl_avl_t *tree, scl_visit_func_t visit, void *ctx)
{
    if (!tree || !visit) return SCL_ERR_NULL_PTR;
    scl_avl_node_t *stack[256];
    int sp = -1;
    scl_avl_node_t *cur = tree->root;
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
