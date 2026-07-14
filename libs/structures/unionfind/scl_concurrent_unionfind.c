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

/* Thread-safe unionfind data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_unionfind.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cunionfind_init(scl_allocator_t *alloc,
                                scl_concurrent_unionfind_t *uf, size_t count) {
  if (scl_unlikely(!uf))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count == 0))
    return SCL_ERR_INVALID_ARG;
  uf->parent =
      scl_alloc(alloc, count * sizeof(atomic_uint), alignof(max_align_t));
  if (scl_unlikely(!uf->parent))
    return SCL_ERR_OUT_OF_MEMORY;
  uf->rank =
      scl_alloc(alloc, count * sizeof(atomic_uint), alignof(max_align_t));
  if (scl_unlikely(!uf->rank)) {
    scl_free(alloc, uf->parent);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < count; i++) {
    atomic_init(&uf->parent[i], (unsigned int)i);
    atomic_init(&uf->rank[i], 0);
  }
  atomic_init(&uf->sets, (unsigned int)count);
  uf->count = count;
  return SCL_OK;
}

void scl_cunionfind_destroy(scl_allocator_t *alloc,
                            scl_concurrent_unionfind_t *uf) {
  if (scl_unlikely(!uf))
    return;
  scl_free(alloc, uf->parent);
  scl_free(alloc, uf->rank);
  uf->parent = NULL;
  uf->rank = NULL;
  uf->count = 0;
  atomic_store_explicit(&uf->sets, 0, memory_order_relaxed);
}

size_t scl_cunionfind_find(scl_concurrent_unionfind_t *uf, size_t x) {
  if (!uf || x >= uf->count)
    return (size_t)-1;
  while (1) {
    unsigned int p = atomic_load_explicit(&uf->parent[x], memory_order_acquire);
    if (p == (unsigned int)x)
      return x;
    unsigned int gp =
        atomic_load_explicit(&uf->parent[p], memory_order_acquire);
    atomic_compare_exchange_strong_explicit(
        &uf->parent[x], &p, gp, memory_order_release, memory_order_relaxed);
    x = p;
  }
}

scl_error_t scl_cunionfind_union(scl_concurrent_unionfind_t *uf, size_t x,
                                 size_t y) {
  if (scl_unlikely(!uf))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(x >= uf->count || y >= uf->count))
    return SCL_ERR_INVALID_INDEX;
  while (1) {
    size_t rx = scl_cunionfind_find(uf, x);
    size_t ry = scl_cunionfind_find(uf, y);
    if (rx == ry)
      return SCL_OK;
    unsigned int rank_rx =
        atomic_load_explicit(&uf->rank[rx], memory_order_relaxed);
    unsigned int rank_ry =
        atomic_load_explicit(&uf->rank[ry], memory_order_relaxed);
    if (rank_rx < rank_ry) {
      if (atomic_compare_exchange_strong_explicit(
              &uf->parent[rx], (unsigned int *)&rx, (unsigned int)ry,
              memory_order_release, memory_order_relaxed)) {
        atomic_fetch_sub_explicit(&uf->sets, 1, memory_order_relaxed);
        return SCL_OK;
      }
    } else if (rank_rx > rank_ry) {
      if (atomic_compare_exchange_strong_explicit(
              &uf->parent[ry], (unsigned int *)&ry, (unsigned int)rx,
              memory_order_release, memory_order_relaxed)) {
        atomic_fetch_sub_explicit(&uf->sets, 1, memory_order_relaxed);
        return SCL_OK;
      }
    } else {
      if (atomic_compare_exchange_strong_explicit(
              &uf->parent[ry], (unsigned int *)&ry, (unsigned int)rx,
              memory_order_release, memory_order_relaxed)) {
        atomic_fetch_add_explicit(&uf->rank[rx], 1, memory_order_relaxed);
        atomic_fetch_sub_explicit(&uf->sets, 1, memory_order_relaxed);
        return SCL_OK;
      }
    }
  }
}

bool scl_cunionfind_connected(scl_concurrent_unionfind_t *uf, size_t x,
                              size_t y) {
  if (scl_unlikely(!uf || x >= uf->count || y >= uf->count))
    return false;
  return scl_cunionfind_find(uf, x) == scl_cunionfind_find(uf, y);
}

size_t scl_cunionfind_count(const scl_concurrent_unionfind_t *uf) {
  return uf ? uf->count : 0;
}

size_t scl_cunionfind_sets(const scl_concurrent_unionfind_t *uf) {
  return uf ? atomic_load_explicit(&uf->sets, memory_order_relaxed) : 0;
}
