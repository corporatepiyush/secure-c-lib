#include "scl_concurrent_bst.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

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
    if (scl_unlikely(!tree)) return;
    scl_spinlock_lock(&tree->lock);
    if (tree->root) {
        size_t cap = 64;
        scl_concurrent_bst_node_t **stack = scl_alloc(alloc, cap * sizeof(stack[0]), alignof(max_align_t));
        if (!stack) { scl_spinlock_unlock(&tree->lock); return; }
        int sp = -1;
        scl_concurrent_bst_node_t *cur = tree->root;
        scl_concurrent_bst_node_t *last = NULL;
        while (cur || sp >= 0) {
            while (scl_likely(cur)) {
                if ((size_t)(sp + 1) >= cap) {
                    size_t new_cap = cap * 2;
                    scl_concurrent_bst_node_t **ns = scl_realloc(alloc, stack, cap * sizeof(stack[0]), new_cap * sizeof(stack[0]), alignof(max_align_t));
                    if (!ns) { scl_free(alloc, stack); scl_spinlock_unlock(&tree->lock); return; }
                    stack = ns;
                    cap = new_cap;
                }
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
        scl_free(alloc, stack);
    }
    tree->root = NULL;
    atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
    scl_spinlock_unlock(&tree->lock);
}

scl_error_t scl_cbst_init(scl_allocator_t *alloc, scl_concurrent_bst_t *tree, size_t element_size,
                         scl_cmp_func_t cmp)
{
    (void)alloc;
    if (scl_unlikely(!tree)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0 || !cmp)) return SCL_ERR_INVALID_ARG;
    tree->root = NULL;
    tree->element_size = element_size;
    atomic_init(&tree->count, 0);
    tree->cmp = cmp;
    scl_spinlock_init(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_cbst_insert(scl_allocator_t *alloc, scl_concurrent_bst_t *tree, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!tree || !element)) return SCL_ERR_NULL_PTR;
    scl_concurrent_bst_node_t *n = create_node(alloc, element, tree->element_size);
    if (scl_unlikely(!n)) return SCL_ERR_OUT_OF_MEMORY;
    scl_spinlock_lock(&tree->lock);
    if (!tree->root) {
        tree->root = n;
        atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
        scl_spinlock_unlock(&tree->lock);
        return SCL_OK;
    }
    scl_concurrent_bst_node_t *cur = tree->root;
    scl_concurrent_bst_node_t *parent = NULL;
    while (scl_likely(cur)) {
        parent = cur;
        int c = tree->cmp(element, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else {
            scl_memcpy(cur->data, element, tree->element_size);
            scl_free(alloc, n->data); scl_free(alloc, n);
            scl_spinlock_unlock(&tree->lock);
            return SCL_OK;
        }
    }
    if (tree->cmp(element, parent->data) < 0)
        parent->left = n;
    else
        parent->right = n;
    atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_cbst_remove(scl_allocator_t *alloc, scl_concurrent_bst_t *tree, const void  *SCL_RESTRICT key)
{
    if (scl_unlikely(!tree || !key)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&tree->lock);

    scl_concurrent_bst_node_t *parent = NULL;
    scl_concurrent_bst_node_t *cur = tree->root;
    bool found = false;

    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) { parent = cur; cur = cur->left; }
        else if (c > 0) { parent = cur; cur = cur->right; }
        else { found = true; break; }
    }

    if (!found) { scl_spinlock_unlock(&tree->lock); return SCL_ERR_NOT_FOUND; }

    if (cur->left && cur->right) {
        scl_concurrent_bst_node_t *succ_parent = cur;
        scl_concurrent_bst_node_t *succ = cur->right;
        while (scl_likely(succ->left)) {
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
    scl_spinlock_unlock(&tree->lock);
    return SCL_OK;
}

bool scl_cbst_contains(scl_concurrent_bst_t *tree, const void *key)
{
    if (scl_unlikely(!tree || !key)) return false;
    scl_spinlock_lock(&tree->lock);
    scl_concurrent_bst_node_t *cur = tree->root;
    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { scl_spinlock_unlock(&tree->lock); return true; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    scl_spinlock_unlock(&tree->lock);
    return false;
}

scl_error_t scl_cbst_find(scl_concurrent_bst_t *tree, const void *key, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!tree || !key || !out)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&tree->lock);
    scl_concurrent_bst_node_t *cur = tree->root;
    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c == 0) { scl_memcpy(out, cur->data, tree->element_size); scl_spinlock_unlock(&tree->lock); return SCL_OK; }
        cur = (c < 0) ? cur->left : cur->right;
    }
    scl_spinlock_unlock(&tree->lock);
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
