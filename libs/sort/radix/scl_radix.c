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

/* LSD radix sort. O(d(N+k)). Non-comparison. Stable digit-by-digit pass. */

#include "scl_radix.h"
#include "scl_string.h"

scl_error_t scl_sort_radix_sort(scl_allocator_t *alloc, int32_t *base,
                                size_t count) {
  if (scl_unlikely(!base))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count < 2))
    return SCL_OK;

  size_t bytes;
  if (scl_unlikely(scl_mul_overflow(count, sizeof(int32_t), &bytes)))
    return SCL_ERR_SIZE_OVERFLOW;
  int32_t *output = (int32_t *)scl_alloc(alloc, bytes, alignof(max_align_t));
  if (scl_unlikely(!output))
    return SCL_ERR_OUT_OF_MEMORY;

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

    scl_memcpy(base, output, bytes);
  }

  scl_free(alloc, output);
  return SCL_OK;
}
