#include "concurrent_rbtree.h"
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

static scl_concurrent_rbtree_node_t *create_node(const void *data, size_t element_size)
{
    scl_concurrent_rbtree_node_t *n = malloc(sizeof(scl_concurrent_rbtree_node_t));
    if (!n) return NULL;
    n->data = malloc(element_size);
    if (!n->data) { free(n); return NULL; }
    memcpy(n->data, data, element_size);
    n->left = n->right = n->parent = NULL;
    n->color = SCL_CONCURRENT_RB_RED;
    return n;
}

static void destroy_subtree(scl_concurrent_rbtree_node_t *n)
{
    if (!n) return;
    destroy_subtree(n->left);
    destroy_subtree(n->right);
    free(n->data);
    free(n);
}

static void rotate_left(scl_concurrent_rbtree_node_t **root, scl_concurrent_rbtree_node_t *x)
{
    scl_concurrent_rbtree_node_t *y = x->right;
    x->right = y->left;
    if (y->left) y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent) *root = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rotate_right(scl_concurrent_rbtree_node_t **root, scl_concurrent_rbtree_node_t *x)
{
    scl_concurrent_rbtree_node_t *y = x->left;
    x->left = y->right;
    if (y->right) y->right->parent = x;
    y->parent = x->parent;
    if (!x->parent) *root = y;
    else if (x == x->parent->right) x->parent->right = y;
    else x->parent->left = y;
    y->right = x;
    x->parent = y;
}

static void insert_fixup(scl_concurrent_rbtree_node_t **root, scl_concurrent_rbtree_node_t *z)
{
    while (z->parent && z->parent->color == SCL_CONCURRENT_RB_RED) {
        if (!z->parent->parent) break;
        if (z->parent == z->parent->parent->left) {
            scl_concurrent_rbtree_node_t *y = z->parent->parent->right;
            if (y && y->color == SCL_CONCURRENT_RB_RED) {
                z->parent->color = SCL_CONCURRENT_RB_BLACK;
                y->color = SCL_CONCURRENT_RB_BLACK;
                z->parent->parent->color = SCL_CONCURRENT_RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) { z = z->parent; rotate_left(root, z); }
                z->parent->color = SCL_CONCURRENT_RB_BLACK;
                z->parent->parent->color = SCL_CONCURRENT_RB_RED;
                rotate_right(root, z->parent->parent);
            }
        } else {
            scl_concurrent_rbtree_node_t *y = z->parent->parent->left;
            if (y && y->color == SCL_CONCURRENT_RB_RED) {
                z->parent->color = SCL_CONCURRENT_RB_BLACK;
                y->color = SCL_CONCURRENT_RB_BLACK;
                z->parent->parent->color = SCL_CONCURRENT_RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) { z = z->parent; rotate_right(root, z); }
                z->parent->color = SCL_CONCURRENT_RB_BLACK;
                z->parent->parent->color = SCL_CONCURRENT_RB_RED;
                rotate_left(root, z->parent->parent);
            }
        }
    }
    (*root)->color = SCL_CONCURRENT_RB_BLACK;
}

static void transplant(scl_concurrent_rbtree_node_t **root, scl_concurrent_rbtree_node_t *u, scl_concurrent_rbtree_node_t *v)
{
    if (!u->parent) *root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    if (v) v->parent = u->parent;
}

static scl_concurrent_rbtree_node_t *tree_min(scl_concurrent_rbtree_node_t *n)
{
    while (n->left) n = n->left;
    return n;
}

static void remove_fixup(scl_concurrent_rbtree_node_t **root, scl_concurrent_rbtree_node_t *x, scl_concurrent_rbtree_node_t *xp, scl_concurrent_rbtree_node_t *x_parent)
{
    (void)xp;
    while (x != *root && (!x || x->color == SCL_CONCURRENT_RB_BLACK)) {
        scl_concurrent_rbtree_node_t *parent = x_parent;
        if (!parent) break;
        if (x == parent->left) {
            scl_concurrent_rbtree_node_t *w = parent->right;
            if (w && w->color == SCL_CONCURRENT_RB_RED) {
                w->color = SCL_CONCURRENT_RB_BLACK;
                parent->color = SCL_CONCURRENT_RB_RED;
                rotate_left(root, parent);
                w = parent->right;
            }
            if (w && (!w->left || w->left->color == SCL_CONCURRENT_RB_BLACK) &&
                     (!w->right || w->right->color == SCL_CONCURRENT_RB_BLACK)) {
                w->color = SCL_CONCURRENT_RB_RED;
                x = parent;
                x_parent = x->parent;
            } else {
                if (w && (!w->right || w->right->color == SCL_CONCURRENT_RB_BLACK)) {
                    if (w->left) w->left->color = SCL_CONCURRENT_RB_BLACK;
                    w->color = SCL_CONCURRENT_RB_RED;
                    rotate_right(root, w);
                    w = parent->right;
                }
                if (w) w->color = parent->color;
                parent->color = SCL_CONCURRENT_RB_BLACK;
                if (w && w->right) w->right->color = SCL_CONCURRENT_RB_BLACK;
                rotate_left(root, parent);
                x = *root;
                x_parent = NULL;
            }
        } else {
            scl_concurrent_rbtree_node_t *w = parent->left;
            if (w && w->color == SCL_CONCURRENT_RB_RED) {
                w->color = SCL_CONCURRENT_RB_BLACK;
                parent->color = SCL_CONCURRENT_RB_RED;
                rotate_right(root, parent);
                w = parent->left;
            }
            if (w && (!w->right || w->right->color == SCL_CONCURRENT_RB_BLACK) &&
                     (!w->left || w->left->color == SCL_CONCURRENT_RB_BLACK)) {
                w->color = SCL_CONCURRENT_RB_RED;
                x = parent;
                x_parent = x->parent;
            } else {
                if (w && (!w->left || w->left->color == SCL_CONCURRENT_RB_BLACK)) {
                    if (w->right) w->right->color = SCL_CONCURRENT_RB_BLACK;
                    w->color = SCL_CONCURRENT_RB_RED;
                    rotate_left(root, w);
                    w = parent->left;
                }
                if (w) w->color = parent->color;
                parent->color = SCL_CONCURRENT_RB_BLACK;
                if (w && w->left) w->left->color = SCL_CONCURRENT_RB_BLACK;
                rotate_right(root, parent);
                x = *root;
                x_parent = NULL;
            }
        }
    }
    if (x) x->color = SCL_CONCURRENT_RB_BLACK;
}

scl_error_t scl_concurrent_rbtree_init(scl_concurrent_rbtree_t *tree, size_t element_size,
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

void scl_concurrent_rbtree_destroy(scl_concurrent_rbtree_t *tree)
{
    if (!tree) return;
    spin_lock(&tree->lock);
    destroy_subtree(tree->root);
    tree->root = NULL;
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
    spin_unlock(&tree->lock);
}

scl_error_t scl_concurrent_rbtree_insert(scl_concurrent_rbtree_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_rbtree_node_t *z = create_node(element, tree->element_size);
    if (!z) return SCL_ERR_OUT_OF_MEMORY;
    spin_lock(&tree->lock);
    scl_concurrent_rbtree_node_t *y = NULL;
    scl_concurrent_rbtree_node_t *x = tree->root;
    while (x) {
        y = x;
        int c = tree->cmp(element, x->data);
        if (c == 0) {
            memcpy(x->data, element, tree->element_size);
            free(z->data); free(z);
            spin_unlock(&tree->lock);
            return SCL_OK;
        }
        x = (c < 0) ? x->left : x->right;
    }
    z->parent = y;
    if (!y) tree->root = z;
    else if (tree->cmp(element, y->data) < 0) y->left = z;
    else y->right = z;
    insert_fixup(&tree->root, z);
    atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_rbtree_remove(scl_concurrent_rbtree_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    scl_concurrent_rbtree_node_t *z = tree->root;
    while (z) {
        int c = tree->cmp(key, z->data);
        if (c == 0) break;
        z = (c < 0) ? z->left : z->right;
    }
    if (!z) { spin_unlock(&tree->lock); return SCL_ERR_NOT_FOUND; }
    scl_concurrent_rbtree_node_t *y = z;
    scl_concurrent_rb_color_t y_orig = y->color;
    scl_concurrent_rbtree_node_t *x = NULL;
    scl_concurrent_rbtree_node_t *x_parent = NULL;
    if (!z->left) {
        x = z->right;
        x_parent = z->parent;
        transplant(&tree->root, z, z->right);
    } else if (!z->right) {
        x = z->left;
        x_parent = z->parent;
        transplant(&tree->root, z, z->left);
    } else {
        y = tree_min(z->right);
        y_orig = y->color;
        x = y->right;
        if (y->parent == z) {
            if (x) x->parent = y;
            x_parent = y;
        } else {
            transplant(&tree->root, y, y->right);
            y->right = z->right;
            y->right->parent = y;
            x_parent = y->parent;
        }
        transplant(&tree->root, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    free(z->data);
    free(z);
    if (y_orig == SCL_CONCURRENT_RB_BLACK)
        remove_fixup(&tree->root, x, NULL, x_parent);
    atomic_fetch_sub_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

bool scl_concurrent_rbtree_contains(scl_concurrent_rbtree_t *tree, const void *key)
{
    if (!tree || !key) return false;
    spin_lock(&tree->lock);
    scl_concurrent_rbtree_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { spin_unlock(&tree->lock); return true; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return false;
}

scl_error_t scl_concurrent_rbtree_find(scl_concurrent_rbtree_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    scl_concurrent_rbtree_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { memcpy(out, cur->data, tree->element_size); spin_unlock(&tree->lock); return SCL_OK; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_concurrent_rbtree_count(const scl_concurrent_rbtree_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_rbtree_empty(const scl_concurrent_rbtree_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) == 0 : true;
}
