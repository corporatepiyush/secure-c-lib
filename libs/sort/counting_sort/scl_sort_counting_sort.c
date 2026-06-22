#include "scl_sort_counting_sort.h"
#include <string.h>

scl_error_t scl_sort_counting_sort(scl_allocator_t *alloc, int32_t *base, size_t count)
{
    if (!base) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    int32_t min_val = base[0], max_val = base[0];
    for (size_t i = 1; i < count; i++) {
        if (base[i] < min_val) min_val = base[i];
        if (base[i] > max_val) max_val = base[i];
    }

    size_t range;
    if (scl_add_overflow((size_t)(max_val - min_val), 1, &range))
        return SCL_ERR_SIZE_OVERFLOW;

    size_t *counts = (size_t *)scl_calloc(alloc, range, sizeof(size_t), alignof(max_align_t));
    if (!counts) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < count; i++)
        counts[(size_t)(base[i] - min_val)]++;

    size_t idx = 0;
    for (size_t i = 0; i < range; i++) {
        for (size_t j = 0; j < counts[i]; j++)
            base[idx++] = (int32_t)((size_t)min_val + i);
    }

    scl_free(alloc, counts);
    return SCL_OK;
}
