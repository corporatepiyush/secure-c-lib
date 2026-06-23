#include "scl_sparse.h"
#include "scl_string.h"

scl_error_t scl_sparse_init(scl_allocator_t *alloc, scl_sparse_t *st,
                             size_t n, size_t element_size, const void *data,
                             void (*combine)(void *out, const void *a, const void *b))
{
    if (!st || !combine || !data) return SCL_ERR_NULL_PTR;
    if (n == 0 || element_size == 0) return SCL_ERR_INVALID_ARG;

    size_t levels_count = 0;
    while ((1UL << levels_count) <= n) levels_count++;

    st->levels = scl_calloc(alloc, levels_count, sizeof(unsigned char *), alignof(max_align_t));
    st->scratch = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!st->levels || !st->scratch) {
        scl_free(alloc, st->levels);
        scl_free(alloc, st->scratch);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    size_t level_sz;
    if (scl_mul_overflow(n, element_size, &level_sz)) {
        scl_free(alloc, st->levels);
        scl_free(alloc, st->scratch);
        return SCL_ERR_SIZE_OVERFLOW;
    }

    size_t ok = 1;
    for (size_t k = 0; k < levels_count; k++) {
        st->levels[k] = scl_alloc(alloc, level_sz, alignof(max_align_t));
        if (!st->levels[k]) { ok = 0; break; }
    }

    if (!ok) {
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

void scl_sparse_destroy(scl_allocator_t *alloc, scl_sparse_t *st)
{
    if (!st) return;
    for (size_t k = 0; k < st->levels_count; k++)
        scl_free(alloc, st->levels[k]);
    scl_free(alloc, st->levels);
    scl_free(alloc, st->scratch);
    st->levels = NULL;
    st->scratch = NULL;
    st->n = 0;
    st->levels_count = 0;
}

scl_error_t scl_sparse_query(const scl_sparse_t *st, size_t l, size_t r, void *out)
{
    if (!st || !out) return SCL_ERR_NULL_PTR;
    if (l > r || r >= st->n) return SCL_ERR_INVALID_ARG;

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
