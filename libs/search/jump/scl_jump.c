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

/* Jump search. O(sqrt(N)). Block-skip then linear scan within block. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_jump.h"
#include "scl_math.h"

scl_error_t scl_search_jump_search(const void *base, size_t count,
                                   size_t elem_size, const void *key,
                                   scl_cmp_func_t cmp, size_t *out_index) {
  if (scl_unlikely(base == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(key == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(cmp == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(out_index == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count == 0))
    return SCL_ERR_EMPTY;

  size_t step = (size_t)scl_sqrt((double)count);
  if (step == 0)
    step = 1;

  size_t prev = 0;
  const unsigned char *bytes = (const unsigned char *)base;

  while (prev < count) {
    size_t block_end = prev + step;
    if (block_end > count)
      block_end = count;
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
    if (block_end == count)
      break;
    prev = block_end;
  }
  return SCL_ERR_NOT_FOUND;
}
