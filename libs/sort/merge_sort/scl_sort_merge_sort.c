#include "scl_sort_merge_sort.h"
#include <string.h>

scl_error_t scl_sort_merge_sort(scl_allocator_t *alloc, void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    size_t bytes;
    if (scl_mul_overflow(count, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!tmp) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t width = 1; width < count; width *= 2) {
        for (size_t i = 0; i < count; i += 2 * width) {
            size_t mid = i + width;
            if (mid >= count) continue;
            size_t r = i + 2 * width;
            if (r > count) r = count;

            size_t li = i, ri = mid, ti = 0;

            while (li < mid && ri < r) {
                if (cmp((unsigned char *)base + li * element_size,
                        (unsigned char *)base + ri * element_size) <= 0)
                    memcpy(tmp + ti++ * element_size,
                           (unsigned char *)base + li++ * element_size,
                           element_size);
                else
                    memcpy(tmp + ti++ * element_size,
                           (unsigned char *)base + ri++ * element_size,
                           element_size);
            }
            while (li < mid)
                memcpy(tmp + ti++ * element_size,
                       (unsigned char *)base + li++ * element_size,
                       element_size);
            while (ri < r)
                memcpy(tmp + ti++ * element_size,
                       (unsigned char *)base + ri++ * element_size,
                       element_size);

            memcpy((unsigned char *)base + i * element_size, tmp, ti * element_size);
        }
    }

    scl_free(alloc, tmp);
    return SCL_OK;
}
