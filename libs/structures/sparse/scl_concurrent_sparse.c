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

/* Thread-safe sparse data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_sparse.h"
#include "scl_string.h"

scl_error_t scl_csparse_init(scl_allocator_t *alloc, scl_concurrent_sparse_t *st,
                             size_t n, size_t element_size, const void *data,
                             void (*combine)(void *out, const void *a, const void  *SCL_RESTRICT b))
{
    if (scl_unlikely(!st || !combine || !data)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(n == 0 || element_size == 0)) return SCL_ERR_INVALID_ARG;

    size_t levels_count = 0;
    while ((1UL << levels_count) <= n) levels_count++;

    st->levels = scl_calloc(alloc, levels_count, sizeof(unsigned char *), alignof(max_align_t));
    st->scratch = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (scl_unlikely(!st->levels || !st->scratch)) {
        scl_free(alloc, st->levels);
        scl_free(alloc, st->scratch);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    size_t ok = 1;
    for (size_t k = 0; k < levels_count; k++) {
        st->levels[k] = scl_alloc(alloc, n * element_size, alignof(max_align_t));
        if (!st->levels[k]) { ok = 0; break; }
    }

    if (scl_unlikely(!ok)) {
        for (size_t k = 0; k < levels_count; k++)
            scl_free(alloc, st->levels[k]);
        scl_free(alloc, st->levels);
        scl_free(alloc, st->scratch);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    st->n = n;
    st->levels_count = levels_count;
    st->element_size = element_size;
    st->combine = combine;

    const unsigned char *src = data;
    scl_memcpy(st->levels[0], src, n * element_size);

    for (size_t k = 1; k < levels_count; k++) {
        size_t len = 1UL << k;
        size_t prev_len = 1UL << (k - 1);
        for (size_t i = 0; i + len <= n; i++) {
            combine(st->levels[k] + i * element_size,
                    st->levels[k - 1] + i * element_size,
                    st->levels[k - 1] + (i + prev_len) * element_size);
        }
    }

    return SCL_OK;
}

void scl_csparse_destroy(scl_allocator_t *alloc, scl_concurrent_sparse_t *st)
{
    if (scl_unlikely(!st)) return;
    for (size_t k = 0; k < st->levels_count; k++)
        scl_free(alloc, st->levels[k]);
    scl_free(alloc, st->levels);
    scl_free(alloc, st->scratch);
    st->levels = NULL;
    st->scratch = NULL;
    st->n = 0;
    st->levels_count = 0;
}

scl_error_t scl_csparse_query(const scl_concurrent_sparse_t *st, size_t l, size_t r, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!st || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(l > r || r >= st->n)) return SCL_ERR_INVALID_ARG;

    size_t len = r - l + 1;
    size_t k = 0;
    while ((1UL << (k + 1)) <= len) k++;

    size_t esize = st->element_size;
    st->combine(st->scratch,
                st->levels[k] + l * esize,
                st->levels[k] + (r - (1UL << k) + 1) * esize);
    scl_memcpy(out, st->scratch, esize);

    return SCL_OK;
}
