#include "scl_concurrent_bst.h"
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

static scl_concurrent_bst_node_t *create_node(scl_allocator_t *alloc, const void *data, size_t element_size)
{
    scl_concurrent_bst_node_t *n = scl_alloc(alloc, sizeof(scl_concurrent_bst_node_t), alignof(max_align_t));
    if (!n) return NULL;
    n->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!n->data) { scl_free(alloc, n); return NULL; }
    scl_memcpy(n->data, data, element_size);
    n->left = NULL;
    n->right = NULL;
    return n;
}

void scl_cbst_destroy(scl_allocator_t *alloc, scl_concurrent_bst_t *tree)
{
    if (!tree) return;
    spin_lock(&tree->lock);
    if (tree->root) {
        scl_concurrent_bst_node_t *stack[256];
        int sp = -1;
        scl_concurrent_bst_node_t *cur = tree->root;
        scl_concurrent_bst_node_t *last = NULL;
        while (cur || sp >= 0) {
            while (cur) {
                stack[++sp] = cur;
                cur = cur->left;
            }
            scl_concurrent_bst_node_t *peek = stack[sp];
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

scl_error_t scl_cbst_init(scl_allocator_t *alloc, scl_concurrent_bst_t *tree, size_t element_size,
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

scl_error_t scl_cbst_insert(scl_allocator_t *alloc, scl_concurrent_bst_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_bst_node_t *n = create_node(alloc, element, tree->element_size);
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
            scl_memcpy(cur->data, element, tree->element_size);
            scl_free(alloc, n->data); scl_free(alloc, n);
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

scl_error_t scl_cbst_remove(scl_allocator_t *alloc, scl_concurrent_bst_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);

    scl_concurrent_bst_node_t *parent = NULL;
    scl_concurrent_bst_node_t *cur = tree->root;
    bool found = false;

    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) { parent = cur; cur = cur->left; }
        else if (c > 0) { parent = cur; cur = cur->right; }
        else { found = true; break; }
    }

    if (!found) { spin_unlock(&tree->lock); return SCL_ERR_NOT_FOUND; }

    if (cur->left && cur->right) {
        scl_concurrent_bst_node_t *succ_parent = cur;
        scl_concurrent_bst_node_t *succ = cur->right;
        while (succ->left) {
            succ_parent = succ;
            succ = succ->left;
        }
        scl_memcpy(cur->data, succ->data, tree->element_size);
        cur = succ;
        parent = succ_parent;
    }

    scl_concurrent_bst_node_t *child = cur->left ? cur->left : cur->right;
    if (!parent)
        tree->root = child;
    else if (parent->left == cur)
        parent->left = child;
    else
        parent->right = child;

    scl_free(alloc, cur->data);
    scl_free(alloc, cur);
    atomic_fetch_sub_explicit(&tree->count, 1, memory_order_relaxed);
    spin_unlock(&tree->lock);
    return SCL_OK;
}

bool scl_cbst_contains(scl_concurrent_bst_t *tree, const void *key)
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

scl_error_t scl_cbst_find(scl_concurrent_bst_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&tree->lock);
    scl_concurrent_bst_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { scl_memcpy(out, cur->data, tree->element_size); spin_unlock(&tree->lock); return SCL_OK; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    spin_unlock(&tree->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_cbst_count(const scl_concurrent_bst_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}

bool scl_cbst_empty(const scl_concurrent_bst_t *tree)
{
    return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) == 0 : true;
}
