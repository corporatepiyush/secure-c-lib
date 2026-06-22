#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_linear_search.h"

scl_error_t scl_search_linear_search(const void *restrict base, size_t count, size_t elem_size, const void *restrict key, scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    for (size_t i = 0; i < count; i++) {
        const void *elem = (const unsigned char *)base + i * elem_size;
        if (__builtin_expect(cmp(elem, key) == 0, 0)) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
