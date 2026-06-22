#include "concurrent_bst.h"
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

static scl_concurrent_bst_node_t *create_node(const void *data, size_t element_size)
{
    scl_concurrent_bst_node_t *n = malloc(sizeof(scl_concurrent_bst_node_t));
    if (!n) return NULL;
    n->data = malloc(element_size);
    if (!n->data) { free(n); return NULL; }
    memcpy(n->data, data, element_size);
    n->left = NULL;
    n->right = NULL;
    return n;
}

static void destroy_subtree(scl_concurrent_bst_node_t *n)
{
    if (!n) return;
    destroy_subtree(n->left);
    destroy_subtree(n->right);
    free(n->data);
    free(n);
}

static scl_concurrent_bst_node_t *find_min(scl_concurrent_bst_node_t *n)
{
    while (n && n->left) n = n->left;
    return n;
}

static scl_concurrent_bst_node_t *remove_node(scl_concurrent_bst_node_t *n, const void *key,
                                               scl_concurrent_cmp_func_t cmp, size_t element_size, bool *found)
{
    if (!n) { *found = false; return NULL; }
    int c = cmp(key, n->data);
    if (c < 0)
        n->left = remove_node(n->left, key, cmp, element_size, found);
    else if (c > 0)
        n->right = remove_node(n->right, key, cmp, element_size, found);
    else {
        *found = true;
        scl_concurrent_bst_node_t *tmp;
        if (!n->left) { tmp = n->right; free(n->data); free(n); return tmp; }
        if (!n->right) { tmp = n->left; free(n->data); free(n); return tmp; }
        tmp = find_min(n->right);
        memcpy(n->data, tmp->data, element_size);
        n->right = remove_node(n->right, tmp->data, cmp, element_size, &(bool){false});
    }
    return n;
}

scl_error_t scl_concurrent_bst_init(scl_concurrent_bst_t *tree, size_t element_size,
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

void scl_concurrent_bst_destroy(scl_concurrent_bst_t *tree)
{
    if (!tree) return;
    spin_lock(&tree->lock);
    destroy_subtree(tree->root);
    tree->root = NULL;
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
    spin_unlock(&tree->lock);
}

scl_error_t scl_concurrent_bst_insert(scl_concurrent_bst_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_bst_node_t *n = create_node(element, tree->element_size);
    if (!n) return SCL_ERR_OUT_OF_MEMORY;
    spin_lock(&tree->lock);
    if (!tree->root) {
        tree->root = n;
        atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
        spin_unlock(&tree->lock);
        return SCL_OK;
    }
    scl_concurrent_bst_node_t *cur = tree->root;
    scl_concurrent_bst_node_t *parent = NULL;
    while (cur) {
        parent = cur;
        int c = tree->cmp(element, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else {
            memcpy(cur->data, element, tree->element_size);
            free(n->data); free(n);
            spin_unlock(&tree->lock);
            return SCL_OK;
        }
    }
    if (tree->cmp(element, parent->data) < 0)
        parent->left = n;
    else
        parent->right = n;
    atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_bst_remove(scl_concurrent_bst_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    bool found = false;
    tree->root = remove_node(tree->root, key, tree->cmp, tree->element_size, &found);
    if (found) atomic_fetch_sub_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return found ? SCL_OK : SCL_ERR_NOT_FOUND;
}

bool scl_concurrent_bst_contains(scl_concurrent_bst_t *tree, const void *key)
{
    if (!tree || !key) return false;
    spin_lock(&tree->lock);
    scl_concurrent_bst_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { spin_unlock(&tree->lock); return true; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return false;
}

scl_error_t scl_concurrent_bst_find(scl_concurrent_bst_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    scl_concurrent_bst_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { memcpy(out, cur->data, tree->element_size); spin_unlock(&tree->lock); return SCL_OK; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_concurrent_bst_count(const scl_concurrent_bst_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_bst_empty(const scl_concurrent_bst_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) == 0 : true;
}
