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

/* Lightweight sharded-array struct — only enough for type declarations.
 * scl_common.h provides the full scl_allocator_t typedef. */

#ifndef SCL_SHARDED_ARRAY_TYPES_H
#define SCL_SHARDED_ARRAY_TYPES_H

#include <stddef.h>
#include <stdint.h>

struct scl_allocator;

#ifndef SCL_SHARDED_ARRAY_TYPE_DEFINED
#define SCL_SHARDED_ARRAY_TYPE_DEFINED
typedef struct {
  struct scl_allocator *alloc;
  unsigned char **shards;
  size_t shard_count;
  size_t dir_cap;
  size_t elem_size;
  size_t shard_len;
  size_t count;
} scl_sharded_array_t;
#endif

#endif