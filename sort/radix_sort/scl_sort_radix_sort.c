#include "scl_sort_radix_sort.h"
#include <string.h>

scl_error_t scl_sort_radix_sort(scl_allocator_t *alloc, int32_t *base, size_t count)
{
    if (!base) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    size_t bytes;
    if (scl_mul_overflow(count, sizeof(int32_t), &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    int32_t *output = (int32_t *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!output) return SCL_ERR_OUT_OF_MEMORY;

    for (int shift = 0; shift < 32; shift += 8) {
        size_t bucket[256] = {0};

        for (size_t i = 0; i < count; i++) {
            int32_t val = base[i] >> shift;
            bucket[(unsigned char)val]++;
        }

        size_t total = 0;
        for (int i = 0; i < 256; i++) {
            size_t old = bucket[i];
            bucket[i] = total;
            total += old;
        }

        for (size_t i = 0; i < count; i++) {
            int32_t val = base[i] >> shift;
            output[bucket[(unsigned char)val]++] = base[i];
        }

        memcpy(base, output, bytes);
    }

    scl_free(alloc, output);
    return SCL_OK;
}
