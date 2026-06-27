/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Thread-safe segtree data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_segtree.h"
#include "scl_string.h"

scl_error_t scl_csegtree_init(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree,
                              size_t n, size_t element_size, const void *data,
                              void (*combine)(void *out, const void *a, const void  *SCL_RESTRICT b))
{
    if (scl_unlikely(!tree || !combine || !data)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(n == 0 || element_size == 0)) return SCL_ERR_INVALID_ARG;

    size_t size = 1;
    while (size < n) size <<= 1;

    tree->data = scl_calloc(alloc, 2 * size, element_size, alignof(max_align_t));
    if (scl_unlikely(!tree->data)) return SCL_ERR_OUT_OF_MEMORY;
    tree->n = n;
    tree->size = size;
    tree->element_size = element_size;
    tree->combine = combine;
    scl_spinlock_init(&tree->lock);

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
    if (scl_unlikely(!tree || !tree->data)) return;
    scl_free(alloc, tree->data);
    tree->data = NULL;
    tree->n = 0;
    tree->size = 0;
}

scl_error_t scl_csegtree_update(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree,
                                size_t idx, const void *val)
{
    (void)alloc;
    if (scl_unlikely(!tree || !val)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(idx >= tree->n)) return SCL_ERR_INVALID_INDEX;

    scl_spinlock_lock(&tree->lock);

    size_t esize = tree->element_size;
    size_t p = tree->size + idx;
    scl_memcpy(tree->data + p * esize, val, esize);

    for (p >>= 1; p >= 1; p >>= 1)
        tree->combine(tree->data + p * esize,
                      tree->data + (2 * p) * esize,
                      tree->data + (2 * p + 1) * esize);

    scl_spinlock_unlock(&tree->lock);
    return SCL_OK;
}

scl_error_t scl_csegtree_query(const scl_concurrent_segtree_t *tree, size_t l, size_t r, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!tree || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(l >= r || r > tree->n)) return SCL_ERR_INVALID_ARG;

    scl_spinlock_lock((scl_spinlock_t *)&tree->lock);

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

    scl_spinlock_unlock((scl_spinlock_t *)&tree->lock);
    return first ? SCL_ERR_EMPTY : SCL_OK;
}
