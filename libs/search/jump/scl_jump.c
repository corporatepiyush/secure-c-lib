#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_jump.h"
#include "scl_math.h"

scl_error_t scl_search_jump_search(const void * base, size_t count, size_t elem_size, const void * key, scl_cmp_func_t cmp, size_t * out_index)
{
    if (scl_unlikely(base == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(cmp == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    size_t step = (size_t)scl_sqrt((double)count);
    if (step == 0) step = 1;

    size_t prev = 0;
    const unsigned char *bytes = (const unsigned char *)base;

    while (prev < count) {
        size_t block_end = prev + step;
        if (block_end > count) block_end = count;
        const void *elem = bytes + (block_end - 1) * elem_size;
        int r = cmp(elem, key);
        if (r >= 0) {
            for (size_t i = prev; i < block_end; i++) {
                const void *e = bytes + i * elem_size;
                if (cmp(e, key) == 0) {
                    *out_index = i;
                    return SCL_OK;
                }
            }
            return SCL_ERR_NOT_FOUND;
        }
        if (block_end == count) break;
        prev = block_end;
    }
    return SCL_ERR_NOT_FOUND;
}
