#include "scl_concurrent_segtree.h"
#include "scl_string.h"

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire))
        scl_cpu_pause();
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

scl_error_t scl_csegtree_init(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree,
                              size_t n, size_t element_size, const void *data,
                              void (*combine)(void *out, const void *a, const void *b))
{
    if (!tree || !combine || !data) return SCL_ERR_NULL_PTR;
    if (n == 0 || element_size == 0) return SCL_ERR_INVALID_ARG;

    size_t size = 1;
    while (size < n) size <<= 1;

    tree->data = scl_calloc(alloc, 2 * size, element_size, alignof(max_align_t));
    if (!tree->data) return SCL_ERR_OUT_OF_MEMORY;
    tree->n = n;
    tree->size = size;
    tree->element_size = element_size;
    tree->combine = combine;
    atomic_flag_clear(&tree->lock);

    const unsigned char *src = data;
    for (size_t i = 0; i < n; i++)
        scl_memcpy(tree->data + (size + i) * element_size, src + i * element_size, element_size);

    for (size_t i = size - 1; i >= 1; i--)
        combine(tree->data + i * element_size,
                tree->data + (2 * i) * element_size,
                tree->data + (2 * i + 1) * element_size);

    return SCL_OK;
}

void scl_csegtree_destroy(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree)
{
    if (!tree || !tree->data) return;
    scl_free(alloc, tree->data);
    tree->data = NULL;
    tree->n = 0;
    tree->size = 0;
}

scl_error_t scl_csegtree_update(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree,
                                size_t idx, const void *val)
{
    (void)alloc;
    if (!tree || !val) return SCL_ERR_NULL_PTR;
    if (idx >= tree->n) return SCL_ERR_INVALID_INDEX;

    spin_lock(&tree->lock);

    size_t esize = tree->element_size;
    size_t p = tree->size + idx;
    scl_memcpy(tree->data + p * esize, val, esize);

    for (p >>= 1; p >= 1; p >>= 1)
        tree->combine(tree->data + p * esize,
                      tree->data + (2 * p) * esize,
                      tree->data + (2 * p + 1) * esize);

    spin_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_csegtree_query(const scl_concurrent_segtree_t *tree, size_t l, size_t r, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (l >= r || r > tree->n) return SCL_ERR_INVALID_ARG;

    spin_lock((atomic_flag *)&tree->lock);

    size_t esize = tree->element_size;
    l += tree->size;
    r += tree->size;

    size_t left_idx[64];
    size_t right_idx[64];
    int li = 0, ri = 0;

    while (l < r) {
        if (l & 1) left_idx[li++] = l++;
        if (r & 1) right_idx[ri++] = --r;
        l >>= 1;
        r >>= 1;
    }

    bool first = true;
    unsigned char *acc = out;
    for (int i = 0; i < li; i++) {
        size_t idx = left_idx[i];
        if (first) {
            scl_memcpy(acc, tree->data + idx * esize, esize);
            first = false;
        } else {
            tree->combine(acc, acc, tree->data + idx * esize);
        }
    }
    for (int i = ri - 1; i >= 0; i--) {
        size_t idx = right_idx[i];
        if (first) {
            scl_memcpy(acc, tree->data + idx * esize, esize);
            first = false;
        } else {
            tree->combine(acc, acc, tree->data + idx * esize);
        }
    }

    spin_unlock((atomic_flag *)&tree->lock);
    return first ? SCL_ERR_EMPTY : SCL_OK;
}
