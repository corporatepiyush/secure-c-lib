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

/* unionfind data structure. */

#include "scl_unionfind.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_unionfind_init(scl_allocator_t *alloc, scl_unionfind_t *uf,
                               size_t count) {
  if (scl_unlikely(!uf))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count == 0))
    return SCL_ERR_INVALID_ARG;

  uf->parent = scl_alloc(alloc, count * sizeof(size_t), alignof(max_align_t));
  uf->rank = scl_calloc(alloc, count, sizeof(size_t), alignof(max_align_t));
  if (scl_unlikely(!uf->parent || !uf->rank)) {
    scl_free(alloc, uf->parent);
    scl_free(alloc, uf->rank);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < count; i++)
    uf->parent[i] = i;

  uf->count = count;
  uf->sets = count;
  return SCL_OK;
}

void scl_unionfind_destroy(scl_allocator_t *alloc, scl_unionfind_t *uf) {
  if (uf) {
    scl_free(alloc, uf->parent);
    scl_free(alloc, uf->rank);
    uf->parent = NULL;
    uf->rank = NULL;
    uf->count = 0;
    uf->sets = 0;
  }
}

size_t scl_unionfind_find(scl_unionfind_t *uf, size_t x) {
  if (!uf || x >= uf->count)
    return SIZE_MAX;

  while (uf->parent[x] != x) {
    uf->parent[x] = uf->parent[uf->parent[x]];
    x = uf->parent[x];
  }
  return x;
}

scl_error_t scl_unionfind_union(scl_unionfind_t *uf, size_t x, size_t y) {
  if (scl_unlikely(!uf))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(x >= uf->count || y >= uf->count))
    return SCL_ERR_INVALID_INDEX;

  size_t rx = scl_unionfind_find(uf, x);
  size_t ry = scl_unionfind_find(uf, y);
  if (rx == ry)
    return SCL_OK;

  if (uf->rank[rx] < uf->rank[ry]) {
    uf->parent[rx] = ry;
  } else if (uf->rank[rx] > uf->rank[ry]) {
    uf->parent[ry] = rx;
  } else {
    uf->parent[ry] = rx;
    uf->rank[rx]++;
  }
  uf->sets--;
  return SCL_OK;
}

bool scl_unionfind_connected(const scl_unionfind_t *uf, size_t x, size_t y) {
  if (!uf || x >= uf->count || y >= uf->count)
    return false;
  return scl_unionfind_find((scl_unionfind_t *)uf, x) ==
         scl_unionfind_find((scl_unionfind_t *)uf, y);
}

size_t scl_unionfind_count(const scl_unionfind_t *uf) {
  return uf ? uf->count : 0;
}
size_t scl_unionfind_sets(const scl_unionfind_t *uf) {
  return uf ? uf->sets : 0;
}
