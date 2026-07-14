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

/* Bucket sort. O(N+k) avg. Distributes into buckets, sorts individually.
 * Stable. */

#include "scl_bucket.h"
#include "scl_string.h"

static SCL_ALWAYS_INLINE void insertion_sort(unsigned char *ptr, size_t count,
                                             size_t element_size,
                                             scl_cmp_func_t cmp) {
  for (size_t i = 1; i < count; i++) {
    size_t j = i;
    while (scl_likely(j > 0) &&
           cmp(ptr + j * element_size, ptr + (j - 1) * element_size) < 0) {
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

scl_error_t scl_sort_bucket_sort(scl_allocator_t *alloc, void *base,
                                 size_t count, size_t element_size,
                                 scl_bucket_key_func_t key,
                                 scl_cmp_func_t cmp) {
  unsigned char *ptr = (unsigned char *)base;
  if (scl_unlikely(!base || !key || !cmp))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count < 2))
    return SCL_OK;

  size_t bucket_count = count < 10 ? count : 10;

  unsigned char **buckets = (unsigned char **)scl_calloc(
      alloc, bucket_count, sizeof(unsigned char *), alignof(max_align_t));
  size_t *bucket_sizes = (size_t *)scl_calloc(
      alloc, bucket_count, sizeof(size_t), alignof(max_align_t));
  size_t *bucket_caps = (size_t *)scl_calloc(
      alloc, bucket_count, sizeof(size_t), alignof(max_align_t));

  if (scl_unlikely(!buckets || !bucket_sizes || !bucket_caps)) {
    scl_free(alloc, buckets);
    scl_free(alloc, bucket_sizes);
    scl_free(alloc, bucket_caps);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  /* O(n) distribution using key extractor.
   * Clamp keys to [0, 1) to prevent out-of-bounds bucket access
   * from adversarial key_func returning negative or >1 values. */
  for (size_t i = 0; i < count; i++) {
    double k = key(ptr + i * element_size);
    if (scl_unlikely(k < 0.0))
      k = 0.0;
    else if (scl_unlikely(k >= 1.0))
      k = 0.999999999;
    size_t bucket_idx = (size_t)(k * bucket_count);
    if (scl_unlikely(bucket_idx >= bucket_count))
      bucket_idx = bucket_count - 1;

    if (bucket_sizes[bucket_idx] == bucket_caps[bucket_idx]) {
      size_t new_cap =
          bucket_caps[bucket_idx] == 0 ? 4 : bucket_caps[bucket_idx] * 2;
      size_t new_bytes;
      if (scl_unlikely(scl_mul_overflow(new_cap, element_size, &new_bytes))) {
        for (size_t j = 0; j < bucket_count; j++)
          scl_free(alloc, buckets[j]);
        scl_free(alloc, buckets);
        scl_free(alloc, bucket_sizes);
        scl_free(alloc, bucket_caps);
        return SCL_ERR_SIZE_OVERFLOW;
      }
      unsigned char *tmp =
          (unsigned char *)scl_alloc(alloc, new_bytes, alignof(max_align_t));
      if (scl_unlikely(!tmp)) {
        for (size_t j = 0; j < bucket_count; j++)
          scl_free(alloc, buckets[j]);
        scl_free(alloc, buckets);
        scl_free(alloc, bucket_sizes);
        scl_free(alloc, bucket_caps);
        return SCL_ERR_OUT_OF_MEMORY;
      }
      if (scl_likely(buckets[bucket_idx] && bucket_sizes[bucket_idx] > 0))
        scl_memcpy(tmp, buckets[bucket_idx],
                   bucket_sizes[bucket_idx] * element_size);
      scl_free(alloc, buckets[bucket_idx]);
      buckets[bucket_idx] = tmp;
      bucket_caps[bucket_idx] = new_cap;
    }

    scl_memcpy(buckets[bucket_idx] + bucket_sizes[bucket_idx] * element_size,
               ptr + i * element_size, element_size);
    bucket_sizes[bucket_idx]++;
  }

  size_t pos = 0;
  for (size_t i = 0; i < bucket_count; i++) {
    if (scl_likely(bucket_sizes[i] > 0)) {
      insertion_sort(buckets[i], bucket_sizes[i], element_size, cmp);
      scl_memcpy(ptr + pos * element_size, buckets[i],
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
