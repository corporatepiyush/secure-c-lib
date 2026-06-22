#include "concurrent_avl.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ volatile("yield");
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static int max_i(int a, int b) { return (a > b) ? a : b; }

static int height(scl_concurrent_avl_node_t *n) { return n ? n->height : 0; }

static int balance(scl_concurrent_avl_node_t *n) { return height(n->left) - height(n->right); }

static scl_concurrent_avl_node_t *rotate_right(scl_concurrent_avl_node_t *y)
{
    scl_concurrent_avl_node_t *x = y->left;
    scl_concurrent_avl_node_t *t = x->right;
    x->right = y;
    y->left = t;
    y->height = 1 + max_i(height(y->left), height(y->right));
    x->height = 1 + max_i(height(x->left), height(x->right));
    return x;
}

static scl_concurrent_avl_node_t *rotate_left(scl_concurrent_avl_node_t *x)
{
    scl_concurrent_avl_node_t *y = x->right;
    scl_concurrent_avl_node_t *t = y->left;
    y->left = x;
    x->right = t;
    x->height = 1 + max_i(height(x->left), height(x->right));
    y->height = 1 + max_i(height(y->left), height(y->right));
    return y;
}

static scl_concurrent_avl_node_t *create_node(const void *data, size_t element_size)
{
    scl_concurrent_avl_node_t *n = malloc(sizeof(scl_concurrent_avl_node_t));
    if (!n) return NULL;
    n->data = malloc(element_size);
    if (!n->data) { free(n); return NULL; }
    memcpy(n->data, data, element_size);
    n->left = n->right = NULL;
    n->height = 1;
    return n;
}

static void destroy_subtree(scl_concurrent_avl_node_t *n)
{
    if (!n) return;
    destroy_subtree(n->left);
    destroy_subtree(n->right);
    free(n->data);
    free(n);
}

static scl_concurrent_avl_node_t *find_min(scl_concurrent_avl_node_t *n)
{
    while (n && n->left) n = n->left;
    return n;
}

static scl_concurrent_avl_node_t *insert_node(scl_concurrent_avl_node_t *n, const void *data,
                                                size_t element_size, scl_concurrent_cmp_func_t cmp,
                                                bool *inserted)
{
    if (!n) {
        *inserted = true;
        return create_node(data, element_size);
    }
    int c = cmp(data, n->data);
    if (c < 0)
        n->left = insert_node(n->left, data, element_size, cmp, inserted);
    else if (c > 0)
        n->right = insert_node(n->right, data, element_size, cmp, inserted);
    else {
        memcpy(n->data, data, element_size);
        *inserted = false;
        return n;
    }
    n->height = 1 + max_i(height(n->left), height(n->right));
    int b = balance(n);
    if (b > 1 && cmp(data, n->left->data) < 0) return rotate_right(n);
    if (b < -1 && cmp(data, n->right->data) > 0) return rotate_left(n);
    if (b > 1 && cmp(data, n->left->data) > 0) {
        n->left = rotate_left(n->left);
        return rotate_right(n);
    }
    if (b < -1 && cmp(data, n->right->data) < 0) {
        n->right = rotate_right(n->right);
        return rotate_left(n);
    }
    return n;
}

static scl_concurrent_avl_node_t *remove_node(scl_concurrent_avl_node_t *n, const void *key,
                                                scl_concurrent_cmp_func_t cmp, size_t element_size, bool *removed)
{
    if (!n) { *removed = false; return NULL; }
    int c = cmp(key, n->data);
    if (c < 0)
        n->left = remove_node(n->left, key, cmp, element_size, removed);
    else if (c > 0)
        n->right = remove_node(n->right, key, cmp, element_size, removed);
    else {
        *removed = true;
        if (!n->left || !n->right) {
            scl_concurrent_avl_node_t *tmp = n->left ? n->left : n->right;
            free(n->data); free(n);
            return tmp;
        }
        scl_concurrent_avl_node_t *tmp = find_min(n->right);
        memcpy(n->data, tmp->data, element_size);
        n->right = remove_node(n->right, tmp->data, cmp, element_size, &(bool){false});
    }
    if (!n) return NULL;
    n->height = 1 + max_i(height(n->left), height(n->right));
    int b = balance(n);
    if (b > 1 && balance(n->left) >= 0) return rotate_right(n);
    if (b > 1 && balance(n->left) < 0) {
        n->left = rotate_left(n->left);
        return rotate_right(n);
    }
    if (b < -1 && balance(n->right) <= 0) return rotate_left(n);
    if (b < -1 && balance(n->right) > 0) {
        n->right = rotate_right(n->right);
        return rotate_left(n);
    }
    return n;
}

scl_error_t scl_concurrent_avl_init(scl_concurrent_avl_t *tree, size_t element_size,
                                    scl_concurrent_cmp_func_t cmp)
{
    if (!tree) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || !cmp) return SCL_ERR_INVALID_ARG;
    tree->root = NULL;
    tree->element_size = element_size;
    atomic_init(&tree->count, 0);
    tree->cmp = cmp;
    atomic_flag_clear(&tree->lock);
    return SCL_OK;
}

void scl_concurrent_avl_destroy(scl_concurrent_avl_t *tree)
{
    if (!tree) return;
    spin_lock(&tree->lock);
    destroy_subtree(tree->root);
    tree->root = NULL;
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
    spin_unlock(&tree->lock);
}

scl_error_t scl_concurrent_avl_insert(scl_concurrent_avl_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    bool inserted = false;
    tree->root = insert_node(tree->root, element, tree->element_size, tree->cmp, &inserted);
    if (inserted) atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_avl_remove(scl_concurrent_avl_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    bool removed = false;
    tree->root = remove_node(tree->root, key, tree->cmp, tree->element_size, &removed);
    if (removed) atomic_fetch_sub_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return removed ? SCL_OK : SCL_ERR_NOT_FOUND;
}

bool scl_concurrent_avl_contains(scl_concurrent_avl_t *tree, const void *key)
{
    if (!tree || !key) return false;
    spin_lock(&tree->lock);
    scl_concurrent_avl_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { spin_unlock(&tree->lock); return true; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return false;
}

scl_error_t scl_concurrent_avl_find(scl_concurrent_avl_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    scl_concurrent_avl_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { memcpy(out, cur->data, tree->element_size); spin_unlock(&tree->lock); return SCL_OK; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_concurrent_avl_count(const scl_concurrent_avl_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_avl_empty(const scl_concurrent_avl_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) == 0 : true;
}
