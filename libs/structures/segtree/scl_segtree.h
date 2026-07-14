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

/* segtree data structure. */

#ifndef SCL_SEGTREE_H
#define SCL_SEGTREE_H

#include "scl_common.h"

typedef struct {
  unsigned char *data;
  size_t n;
  size_t size;
  size_t element_size;
  void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a,
                  const void *SCL_RESTRICT b);
} scl_segtree_t;

scl_error_t scl_segtree_init(
    scl_allocator_t *alloc, scl_segtree_t *tree, size_t n, size_t element_size,
    const void *data,
    void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a,
                    const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void scl_segtree_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                         scl_segtree_t *SCL_RESTRICT tree);
scl_error_t scl_segtree_update(scl_segtree_t *SCL_RESTRICT tree, size_t idx,
                               const void *SCL_RESTRICT val) SCL_WARN_UNUSED;
scl_error_t scl_segtree_query(const scl_segtree_t *SCL_RESTRICT tree, size_t l,
                              size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
