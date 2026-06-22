#include "concurrent_btree.h"
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

static scl_concurrent_btree_node_t *create_node(bool leaf, int t)
{
    scl_concurrent_btree_node_t *n = malloc(sizeof(scl_concurrent_btree_node_t));
    if (!n) return NULL;
    n->keys = calloc(2 * t - 1, sizeof(void *));
    n->values = calloc(2 * t - 1, sizeof(void *));
    n->children = calloc(2 * t, sizeof(scl_concurrent_btree_node_t *));
    if (!n->keys || !n->values || !n->children) {
        free(n->keys); free(n->values); free(n->children); free(n);
        return NULL;
    }
    n->count = 0;
    n->leaf = leaf;
    return n;
}

static void destroy_node(scl_concurrent_btree_node_t *n, int t)
{
    if (!n) return;
    if (!n->leaf) {
        for (size_t i = 0; i < n->count + 1; i++)
            destroy_node(n->children[i], t);
    }
    for (size_t i = 0; i < n->count; i++) {
        free(n->keys[i]);
        free(n->values[i]);
    }
    free(n->keys);
    free(n->values);
    free(n->children);
    free(n);
}

static void split_child(scl_concurrent_btree_node_t *x, int i, int t)
{
    scl_concurrent_btree_node_t *y = x->children[i];
    scl_concurrent_btree_node_t *z = create_node(y->leaf, t);
    z->count = t - 1;
    for (int j = 0; j < t - 1; j++) {
        z->keys[j] = y->keys[j + t];
        z->values[j] = y->values[j + t];
    }
    if (!y->leaf) {
        for (int j = 0; j < t; j++)
            z->children[j] = y->children[j + t];
    }
    y->count = t - 1;
    for (int j = (int)x->count; j >= i + 1; j--)
        x->children[j + 1] = x->children[j];
    x->children[i + 1] = z;
    for (int j = (int)x->count - 1; j >= i; j--) {
        x->keys[j + 1] = x->keys[j];
        x->values[j + 1] = x->values[j];
    }
    x->keys[i] = y->keys[t - 1];
    x->values[i] = y->values[t - 1];
    x->count++;
}

static bool insert_nonfull(scl_concurrent_btree_node_t *x, const void *key, const void *value,
                           size_t key_size, size_t value_size, int t, scl_concurrent_cmp_func_t cmp)
{
    int i;
    for (i = (int)x->count - 1; i >= 0; i--) {
        int c = cmp(key, x->keys[i]);
        if (c == 0) { memcpy(x->values[i], value, value_size); return false; }
        if (c > 0) break;
    }
    i++;
    if (x->leaf) {
        for (int j = (int)x->count - 1; j >= i; j--) {
            x->keys[j + 1] = x->keys[j];
            x->values[j + 1] = x->values[j];
        }
        x->keys[i] = malloc(key_size);
        x->values[i] = malloc(value_size);
        memcpy(x->keys[i], key, key_size);
        memcpy(x->values[i], value, value_size);
        x->count++;
        return true;
    }
    if (x->children[i]->count == (size_t)(2 * t - 1)) {
        split_child(x, i, t);
        int c = cmp(key, x->keys[i]);
        if (c == 0) { memcpy(x->values[i], value, value_size); return false; }
        if (c > 0) i++;
    }
    return insert_nonfull(x->children[i], key, value, key_size, value_size, t, cmp);
}

scl_error_t scl_concurrent_btree_init(scl_concurrent_btree_t *tree, size_t key_size, size_t value_size,
                                      int degree, scl_concurrent_cmp_func_t cmp)
{
    if (!tree) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || degree < 2 || !cmp) return SCL_ERR_INVALID_ARG;
    tree->t = degree;
    tree->root = create_node(true, degree);
    if (!tree->root) return SCL_ERR_OUT_OF_MEMORY;
    tree->key_size = key_size;
    tree->value_size = value_size;
    atomic_init(&tree->count, 0);
    tree->cmp = cmp;
    atomic_flag_clear(&tree->lock);
    return SCL_OK;
}

void scl_concurrent_btree_destroy(scl_concurrent_btree_t *tree)
{
    if (!tree) return;
    spin_lock(&tree->lock);
    destroy_node(tree->root, tree->t);
    tree->root = NULL;
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
    spin_unlock(&tree->lock);
}

scl_error_t scl_concurrent_btree_insert(scl_concurrent_btree_t *tree, const void *key, const void *value)
{
    if (!tree || !key || !value) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    if (tree->root->count == (size_t)(2 * tree->t - 1)) {
        scl_concurrent_btree_node_t *s = create_node(false, tree->t);
        if (!s) { spin_unlock(&tree->lock); return SCL_ERR_OUT_OF_MEMORY; }
        s->children[0] = tree->root;
        tree->root = s;
        split_child(s, 0, tree->t);
    }
    if (insert_nonfull(tree->root, key, value, tree->key_size, tree->value_size, tree->t, tree->cmp))
        atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

static scl_error_t btree_get_helper(scl_concurrent_btree_node_t *x, const void *key,
                                    void *out_value, size_t value_size, scl_concurrent_cmp_func_t cmp)
{
    int i = 0;
    while (i < (int)x->count && cmp(key, x->keys[i]) > 0) i++;
    if (i < (int)x->count && cmp(key, x->keys[i]) == 0) {
        memcpy(out_value, x->values[i], value_size);
        return SCL_OK;
    }
    if (x->leaf) return SCL_ERR_NOT_FOUND;
    return btree_get_helper(x->children[i], key, out_value, value_size, cmp);
}

scl_error_t scl_concurrent_btree_get(const scl_concurrent_btree_t *tree, const void *key, void *out_value)
{
    if (!tree || !key || !out_value) return SCL_ERR_NULL_PTR;
    spin_lock((atomic_flag *)&tree->lock);
    scl_error_t err = btree_get_helper(tree->root, key, out_value, tree->value_size, tree->cmp);
    spin_unlock((atomic_flag *)&tree->lock);
    return err;
}

bool scl_concurrent_btree_contains(const scl_concurrent_btree_t *tree, const void *key)
{
    if (!tree || !key) return false;
    void *tmp = malloc(tree->value_size);
    if (!tmp) return false;
    bool found = btree_get_helper(tree->root, key, tmp, tree->value_size, tree->cmp) == SCL_OK;
    free(tmp);
    return found;
}

scl_error_t scl_concurrent_btree_remove(scl_concurrent_btree_t *tree, const void *key)
{
    (void)tree;
    (void)key;
    return SCL_ERR_NOT_IMPLEMENTED;
}

size_t scl_concurrent_btree_count(const scl_concurrent_btree_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}
