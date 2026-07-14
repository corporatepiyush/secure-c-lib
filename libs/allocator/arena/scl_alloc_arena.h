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

/* Arena (bump) allocator. O(1) sequential allocation via pointer bump.
 * Linked-list of chunks for bulk reset. Ideal for frame-scoped workloads. */

#ifndef SCL_ALLOC_ARENA_H
#define SCL_ALLOC_ARENA_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_allocator_t *scl_alloc_arena_create(scl_allocator_t *backing,
                                         size_t capacity,
                                         size_t max_capacity,
                                         size_t alignment) SCL_WARN_UNUSED;
void scl_alloc_arena_reset(scl_allocator_t *alloc);
void scl_alloc_arena_destroy(scl_allocator_t *alloc);

/* Arena stats (optional introspection) */
typedef struct {
  size_t bytes_used;
  size_t bytes_wasted;
  size_t node_count;
} scl_alloc_arena_stats_t;

bool scl_alloc_arena_stats(scl_allocator_t *alloc,
                           scl_alloc_arena_stats_t *out);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
