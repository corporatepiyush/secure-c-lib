#include "scl_sort_bucket_sort.h"
#include "scl_sort_bucket_sort.h"
#include <string.h>

static void insertion_sort(unsigned char *ptr, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    for (size_t i = 1; i < count; i++) {
        size_t j = i;
        while (j > 0 && cmp(ptr + j * element_size, ptr + (j - 1) * element_size) < 0) {
            size_t k = 0;
            while (k < element_size) {
                unsigned char t = ptr[j * element_size + k];
                ptr[j * element_size + k] = ptr[(j - 1) * element_size + k];
                ptr[(j - 1) * element_size + k] = t;
                k++;
            }
            j--;
        }
    }
}

scl_error_t scl_sort_bucket_sort(scl_allocator_t *alloc, void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    unsigned char *ptr = (unsigned char *)base;
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    size_t bucket_count = count < 10 ? count : 10;

    size_t ptr_bytes;
    if (scl_mul_overflow(bucket_count, sizeof(unsigned char *), &ptr_bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    unsigned char **buckets = (unsigned char **)scl_calloc(alloc, bucket_count, sizeof(unsigned char *), alignof(max_align_t));
    size_t *bucket_sizes = (size_t *)scl_calloc(alloc, bucket_count, sizeof(size_t), alignof(max_align_t));
    size_t *bucket_caps = (size_t *)scl_calloc(alloc, bucket_count, sizeof(size_t), alignof(max_align_t));

    if (!buckets || !bucket_sizes || !bucket_caps) {
        scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    size_t *less_count = (size_t *)scl_alloc(alloc, count * sizeof(size_t), alignof(max_align_t));
    if (!less_count) {
        scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        less_count[i] = 0;
        for (size_t j = 0; j < count; j++) {
            if (cmp(ptr + i * element_size, ptr + j * element_size) > 0)
                less_count[i]++;
        }
    }

    for (size_t i = 0; i < count; i++) {
        size_t bucket_idx = (less_count[i] * bucket_count) / count;
        if (bucket_idx >= bucket_count) bucket_idx = bucket_count - 1;

        if (bucket_sizes[bucket_idx] == bucket_caps[bucket_idx]) {
            size_t new_cap = bucket_caps[bucket_idx] == 0 ? 4 : bucket_caps[bucket_idx] * 2;
            size_t new_bytes;
            if (scl_mul_overflow(new_cap, element_size, &new_bytes)) {
                for (size_t j = 0; j < bucket_count; j++) scl_free(alloc, buckets[j]);
                scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps); scl_free(alloc, less_count);
                return SCL_ERR_SIZE_OVERFLOW;
            }
            unsigned char *tmp = (unsigned char *)scl_alloc(alloc, new_bytes, alignof(max_align_t));
            if (!tmp) {
                for (size_t j = 0; j < bucket_count; j++) scl_free(alloc, buckets[j]);
                scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps); scl_free(alloc, less_count);
                return SCL_ERR_OUT_OF_MEMORY;
            }
            if (buckets[bucket_idx] && bucket_sizes[bucket_idx] > 0)
                memcpy(tmp, buckets[bucket_idx], bucket_sizes[bucket_idx] * element_size);
            scl_free(alloc, buckets[bucket_idx]);
            buckets[bucket_idx] = tmp;
            bucket_caps[bucket_idx] = new_cap;
        }

        memcpy(buckets[bucket_idx] + bucket_sizes[bucket_idx] * element_size,
               ptr + i * element_size, element_size);
        bucket_sizes[bucket_idx]++;
    }

    scl_free(alloc, less_count);

    size_t pos = 0;
    for (size_t i = 0; i < bucket_count; i++) {
        if (bucket_sizes[i] > 0) {
            insertion_sort(buckets[i], bucket_sizes[i], element_size, cmp);
            memcpy(ptr + pos * element_size, buckets[i],
                   bucket_sizes[i] * element_size);
            pos += bucket_sizes[i];
        }
        scl_free(alloc, buckets[i]);
    }

    scl_free(alloc, buckets);
    scl_free(alloc, bucket_sizes);
    scl_free(alloc, bucket_caps);
    return SCL_OK;
}
