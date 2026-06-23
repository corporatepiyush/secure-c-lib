#include "scl_concurrent_btree.h"
#include "scl_string.h"

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

static scl_concurrent_btree_node_t *create_node(scl_allocator_t *alloc, bool leaf, int t,
                                             size_t key_size, size_t value_size)
{
    scl_concurrent_btree_node_t *n = scl_alloc(alloc, sizeof(scl_concurrent_btree_node_t), alignof(max_align_t));
    if (!n) return NULL;
    n->keys = scl_calloc(alloc, 2 * t - 1, sizeof(void *), alignof(max_align_t));
    n->values = scl_calloc(alloc, 2 * t - 1, sizeof(void *), alignof(max_align_t));
    n->children = scl_calloc(alloc, 2 * t, sizeof(scl_concurrent_btree_node_t *), alignof(max_align_t));
    if (!n->keys || !n->values || !n->children) {
        scl_free(alloc, n->keys); scl_free(alloc, n->values); scl_free(alloc, n->children); scl_free(alloc, n);
        return NULL;
    }
    for (size_t i = 0; i < (size_t)(2 * t - 1); i++) {
        n->keys[i] = scl_alloc(alloc, key_size, alignof(max_align_t));
        n->values[i] = scl_alloc(alloc, value_size, alignof(max_align_t));
    }
    n->count = 0;
    n->leaf = leaf;
    return n;
}

void scl_cbtree_destroy(scl_allocator_t *alloc, scl_concurrent_btree_t *tree)
{
    if (!tree || !tree->root) return;

    scl_concurrent_btree_node_t *stack[256];
    int sp = 0;
    stack[sp++] = tree->root;
    scl_concurrent_btree_node_t *stack2[256];
    int sp2 = 0;

    while (sp > 0) {
        scl_concurrent_btree_node_t *node = stack[--sp];
        stack2[sp2++] = node;
        if (!node->leaf) {
            for (size_t i = 0; i <= node->count; i++)
                if (node->children[i])
                    stack[sp++] = node->children[i];
        }
    }

    while (sp2 > 0) {
        scl_concurrent_btree_node_t *node = stack2[--sp2];
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
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
}

static void split_child(scl_allocator_t *alloc, scl_concurrent_btree_node_t *x, int i, int t,
                         size_t key_size, size_t value_size)
{
    scl_concurrent_btree_node_t *y = x->children[i];
    scl_concurrent_btree_node_t *z = create_node(alloc, y->leaf, t, key_size, value_size);
    z->count = t - 1;
    for (int j = 0; j < t - 1; j++) {
        scl_memcpy(z->keys[j], y->keys[j + t], key_size);
        scl_memcpy(z->values[j], y->values[j + t], value_size);
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
        scl_memcpy(x->keys[j + 1], x->keys[j], key_size);
        scl_memcpy(x->values[j + 1], x->values[j], value_size);
    }
    scl_memcpy(x->keys[i], y->keys[t - 1], key_size);
    scl_memcpy(x->values[i], y->values[t - 1], value_size);
    x->count++;
}

scl_error_t scl_cbtree_init(scl_allocator_t *alloc, scl_concurrent_btree_t *tree, size_t key_size, size_t value_size,
                           int degree, scl_cmp_func_t cmp)
{
    if (!tree) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || degree < 2 || !cmp) return SCL_ERR_INVALID_ARG;
    tree->t = degree;
    tree->root = create_node(alloc, true, degree, key_size, value_size);
    if (!tree->root) return SCL_ERR_OUT_OF_MEMORY;
    tree->key_size = key_size;
    tree->value_size = value_size;
    atomic_init(&tree->count, 0);
    tree->cmp = cmp;
    atomic_flag_clear(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_cbtree_insert(scl_allocator_t *alloc, scl_concurrent_btree_t *tree, const void *key, const void *value)
{
    if (!tree || !key || !value) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);

    if (tree->root->count == (size_t)(2 * tree->t - 1)) {
        scl_concurrent_btree_node_t *s = create_node(alloc, false, tree->t, tree->key_size, tree->value_size);
        if (!s) { spin_unlock(&tree->lock); return SCL_ERR_OUT_OF_MEMORY; }
        s->children[0] = tree->root;
        tree->root = s;
        split_child(alloc, s, 0, tree->t, tree->key_size, tree->value_size);
    }

    scl_concurrent_btree_node_t *node = tree->root;
    bool inserted = false;

    while (1) {
        int i = (int)node->count - 1;
        if (node->leaf) {
            while (i >= 0 && tree->cmp(key, node->keys[i]) < 0) {
                scl_memcpy(node->keys[i + 1], node->keys[i], tree->key_size);
                scl_memcpy(node->values[i + 1], node->values[i], tree->value_size);
                i--;
            }
            i++;
            if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
                scl_memcpy(node->values[i], value, tree->value_size);
                spin_unlock(&tree->lock);
                return SCL_OK;
            }
            scl_memcpy(node->keys[i], key, tree->key_size);
            scl_memcpy(node->values[i], value, tree->value_size);
            node->count++;
            inserted = true;
            break;
        } else {
            while (i >= 0 && tree->cmp(key, node->keys[i]) < 0)
                i--;
            i++;
            if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
                scl_memcpy(node->values[i], value, tree->value_size);
                spin_unlock(&tree->lock);
                return SCL_OK;
            }
            if (node->children[i]->count == (size_t)(2 * tree->t - 1)) {
                split_child(alloc, node, i, tree->t, tree->key_size, tree->value_size);
                if (tree->cmp(key, node->keys[i]) > 0)
                    i++;
            }
            node = node->children[i];
        }
    }

    if (inserted)
        atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_cbtree_get(const scl_concurrent_btree_t *tree, const void *key, void *out_value)
{
    if (!tree || !key || !out_value) return SCL_ERR_NULL_PTR;

    scl_concurrent_btree_node_t *node = tree->root;
    while (node) {
        int i = 0;
        while (i < (int)node->count && tree->cmp(key, node->keys[i]) > 0)
            i++;
        if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
            scl_memcpy(out_value, node->values[i], tree->value_size);
            return SCL_OK;
        }
        if (node->leaf) break;
        node = node->children[i];
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_cbtree_contains(const scl_concurrent_btree_t *tree, const void *key)
{
    if (!tree || !key) return false;
    scl_concurrent_btree_node_t *node = tree->root;
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

scl_error_t scl_cbtree_remove(scl_allocator_t *alloc, scl_concurrent_btree_t *tree, const void *key)
{
    (void)alloc;
    (void)tree;
    (void)key;
    return SCL_ERR_NOT_IMPLEMENTED;
}

size_t scl_cbtree_count(const scl_concurrent_btree_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}
