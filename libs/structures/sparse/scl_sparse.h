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

/* sparse data structure. */

#ifndef SCL_SPARSE_H
#define SCL_SPARSE_H

#include "scl_common.h"

typedef struct {
  unsigned char **levels;
  size_t n;
  size_t levels_count;
  size_t element_size;
  unsigned char *scratch;
  void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a,
                  const void *SCL_RESTRICT b);
} scl_sparse_t;

scl_error_t scl_sparse_init(
    scl_allocator_t *alloc, scl_sparse_t *st, size_t n, size_t element_size,
    const void *data,
    void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a,
                    const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void scl_sparse_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                        scl_sparse_t *SCL_RESTRICT st);
scl_error_t scl_sparse_query(const scl_sparse_t *SCL_RESTRICT st, size_t l,
                             size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
