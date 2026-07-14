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

#ifndef SCL_UNIONFIND_H
#define SCL_UNIONFIND_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
  size_t *parent;
  size_t *rank;
  size_t count;
  size_t sets;
} scl_unionfind_t;

scl_error_t scl_unionfind_init(scl_allocator_t *SCL_RESTRICT alloc,
                               scl_unionfind_t *SCL_RESTRICT uf,
                               size_t count) SCL_WARN_UNUSED;
void scl_unionfind_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                           scl_unionfind_t *SCL_RESTRICT uf);
size_t scl_unionfind_find(scl_unionfind_t *SCL_RESTRICT uf, size_t x);
scl_error_t scl_unionfind_union(scl_unionfind_t *SCL_RESTRICT uf, size_t x,
                                size_t y) SCL_WARN_UNUSED;
bool scl_unionfind_connected(const scl_unionfind_t *SCL_RESTRICT uf, size_t x,
                             size_t y);
SCL_PURE size_t scl_unionfind_count(const scl_unionfind_t *SCL_RESTRICT uf);
size_t scl_unionfind_sets(const scl_unionfind_t *SCL_RESTRICT uf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
