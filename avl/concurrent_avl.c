#include "concurrent_avl.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        scl_cpu_pause();
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static int max_i(int a, int b) { return (a > b) ? a : b; }

static int height(scl_atomic_avl_node_t *n) { return n ? n->height : 0; }

static int balance(scl_atomic_avl_node_t *n) { return height(n->left) - height(n->right); }

static scl_atomic_avl_node_t *rotate_right(scl_atomic_avl_node_t *y)
{
    scl_atomic_avl_node_t *x = y->left;
    scl_atomic_avl_node_t *t = x->right;
    x->right = y;
    y->left = t;
    y->height = 1 + max_i(height(y->left), height(y->right));
    x->height = 1 + max_i(height(x->left), height(x->right));
    return x;
}

static scl_atomic_avl_node_t *rotate_left(scl_atomic_avl_node_t *x)
{
    scl_atomic_avl_node_t *y = x->right;
    scl_atomic_avl_node_t *t = y->left;
    y->left = x;
    x->right = t;
    x->height = 1 + max_i(height(x->left), height(x->right));
    y->height = 1 + max_i(height(y->left), height(y->right));
    return y;
}

static scl_atomic_avl_node_t *create_node(scl_allocator_t *alloc, const void *data, size_t element_size)
{
    scl_atomic_avl_node_t *n = scl_alloc(alloc, sizeof(scl_atomic_avl_node_t), alignof(max_align_t));
    if (!n) return NULL;
    n->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!n->data) { scl_free(alloc, n); return NULL; }
    memcpy(n->data, data, element_size);
    n->left = n->right = NULL;
    n->height = 1;
    return n;
}

void scl_atomic_avl_destroy(scl_allocator_t *alloc, scl_atomic_avl_t *tree)
{
    if (!tree) return;
    spin_lock(&tree->lock);
    if (tree->root) {
        scl_atomic_avl_node_t *stack[256];
        int sp = -1;
        scl_atomic_avl_node_t *cur = tree->root;
        scl_atomic_avl_node_t *last = NULL;
        while (cur || sp >= 0) {
            while (cur) {
                stack[++sp] = cur;
                cur = cur->left;
            }
            scl_atomic_avl_node_t *peek = stack[sp];
            if (peek->right && last != peek->right) {
                cur = peek->right;
            } else {
                sp--;
                scl_free(alloc, peek->data);
                scl_free(alloc, peek);
                last = peek;
            }
        }
    }
    tree->root = NULL;
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
    spin_unlock(&tree->lock);
}

scl_error_t scl_atomic_avl_init(scl_allocator_t *alloc, scl_atomic_avl_t *tree, size_t element_size,
                         scl_cmp_func_t cmp)
{
    (void)alloc;
    if (!tree) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || !cmp) return SCL_ERR_INVALID_ARG;
    tree->root = NULL;
    tree->element_size = element_size;
    atomic_init(&tree->count, 0);
    tree->cmp = cmp;
    atomic_flag_clear(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_atomic_avl_insert(scl_allocator_t *alloc, scl_atomic_avl_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);

    scl_atomic_avl_node_t *node = create_node(alloc, element, tree->element_size);
    if (!node) { spin_unlock(&tree->lock); return SCL_ERR_OUT_OF_MEMORY; }

    if (!tree->root) {
        tree->root = node;
        atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
        spin_unlock(&tree->lock);
        return SCL_OK;
    }

    scl_atomic_avl_node_t *path[256];
    int path_len = 0;
    scl_atomic_avl_node_t *cur = tree->root;

    while (1) {
        path[path_len++] = cur;
        int c = tree->cmp(element, cur->data);
        if (c < 0) {
            if (!cur->left) { cur->left = node; break; }
            cur = cur->left;
        } else if (c > 0) {
            if (!cur->right) { cur->right = node; break; }
            cur = cur->right;
        } else {
            memcpy(cur->data, element, tree->element_size);
            scl_free(alloc, node->data); scl_free(alloc, node);
            spin_unlock(&tree->lock);
            return SCL_OK;
        }
    }

    for (int i = path_len - 1; i >= 0; i--) {
        scl_atomic_avl_node_t *n = path[i];
        n->height = 1 + max_i(height(n->left), height(n->right));
        int b = balance(n);
        scl_atomic_avl_node_t *new_sub;

        if (b > 1) {
            if (balance(n->left) >= 0)
                new_sub = rotate_right(n);
            else {
                n->left = rotate_left(n->left);
                new_sub = rotate_right(n);
            }
        } else if (b < -1) {
            if (balance(n->right) <= 0)
                new_sub = rotate_left(n);
            else {
                n->right = rotate_right(n->right);
                new_sub = rotate_left(n);
            }
        } else {
            continue;
        }

        if (i == 0)
            tree->root = new_sub;
        else {
            scl_atomic_avl_node_t *p = path[i - 1];
            if (p->left == n) p->left = new_sub;
            else p->right = new_sub;
        }
    }

    atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_atomic_avl_remove(scl_allocator_t *alloc, scl_atomic_avl_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);

    if (!tree->root) { spin_unlock(&tree->lock); return SCL_ERR_NOT_FOUND; }

    scl_atomic_avl_node_t *path[256];
    int path_len = 0;
    scl_atomic_avl_node_t *cur = tree->root;
    bool found = false;

    while (cur) {
        path[path_len++] = cur;
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else { found = true; break; }
    }

    if (!found) { spin_unlock(&tree->lock); return SCL_ERR_NOT_FOUND; }

    scl_atomic_avl_node_t *node = path[--path_len];

    if (!node->left || !node->right) {
        scl_atomic_avl_node_t *child = node->left ? node->left : node->right;
        if (path_len == 0)
            tree->root = child;
        else {
            scl_atomic_avl_node_t *p = path[path_len - 1];
            if (p->left == node) p->left = child;
            else p->right = child;
        }
        scl_free(alloc, node->data);
        scl_free(alloc, node);
    } else {
        scl_atomic_avl_node_t *succ = node->right;
        int succ_idx = path_len;
        while (succ->left) {
            if (succ_idx == path_len) path[succ_idx] = node;
            path[succ_idx++] = succ;
            succ = succ->left;
        }
        if (succ_idx == path_len) path[succ_idx] = node;
        path[succ_idx++] = succ;

        memcpy(node->data, succ->data, tree->element_size);

        scl_atomic_avl_node_t *succ_parent = path[succ_idx - 1];
        scl_atomic_avl_node_t *succ_child = succ->right;
        if (succ_parent->left == succ)
            succ_parent->left = succ_child;
        else
            succ_parent->right = succ_child;
        scl_free(alloc, succ->data);
        scl_free(alloc, succ);

        path_len = succ_idx - 1;
    }

    for (int i = path_len - 1; i >= 0; i--) {
        scl_atomic_avl_node_t *n = path[i];
        n->height = 1 + max_i(height(n->left), height(n->right));
        int b = balance(n);
        scl_atomic_avl_node_t *new_sub;

        if (b > 1) {
            if (balance(n->left) >= 0)
                new_sub = rotate_right(n);
            else {
                n->left = rotate_left(n->left);
                new_sub = rotate_right(n);
            }
        } else if (b < -1) {
            if (balance(n->right) <= 0)
                new_sub = rotate_left(n);
            else {
                n->right = rotate_right(n->right);
                new_sub = rotate_left(n);
            }
        } else {
            continue;
        }

        if (i == 0)
            tree->root = new_sub;
        else {
            scl_atomic_avl_node_t *p = path[i - 1];
            if (p->left == n) p->left = new_sub;
            else p->right = new_sub;
        }
    }

    atomic_fetch_sub_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

bool scl_atomic_avl_contains(scl_atomic_avl_t *tree, const void *key)
{
    if (!tree || !key) return false;
    spin_lock(&tree->lock);
    scl_atomic_avl_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { spin_unlock(&tree->lock); return true; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return false;
}

scl_error_t scl_atomic_avl_find(scl_atomic_avl_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    scl_atomic_avl_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { memcpy(out, cur->data, tree->element_size); spin_unlock(&tree->lock); return SCL_OK; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_atomic_avl_count(const scl_atomic_avl_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}

bool scl_atomic_avl_empty(const scl_atomic_avl_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) == 0 : true;
}
