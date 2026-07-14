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

/* Meta-binary search. O(log N). Recursive bit-probing variant. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_meta_binary.h"

scl_error_t scl_search_meta_binary_search(const int32_t *arr, size_t count,
                                          int32_t key, size_t *out_index) {
  if (scl_unlikely(arr == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(out_index == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count == 0))
    return SCL_ERR_EMPTY;

  size_t bit_count = 0;
  size_t tmp = count;
  while (tmp) {
    bit_count++;
    tmp >>= 1;
  }

  size_t pos = 0;
  for (size_t b = bit_count; b > 0; b--) {
    size_t step = 1ULL << (b - 1);
    size_t mid = pos + step;
    if (mid >= count)
      continue;
    if (arr[mid] <= key)
      pos = mid;
  }

  if (arr[pos] == key) {
    *out_index = pos;
    return SCL_OK;
  }
  return SCL_ERR_NOT_FOUND;
}
