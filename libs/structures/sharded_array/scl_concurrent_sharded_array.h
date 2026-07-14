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

/* Thread-safe sharded array backed by a linked list of shards.
 * Each shard holds a fixed number of elements; new shards are appended
 * to the linked list when the current one fills. A spinlock protects
 * all mutation and shard-list traversal for concurrent use. */

#ifndef SCL_CONCURRENT_SHARDED_ARRAY_H
#define SCL_CONCURRENT_SHARDED_ARRAY_H

#include "scl_atomic.h"
#include "scl_common.h"
#include "scl_concurrent_common.h"

/* Default elements per shard when the caller passes 0. */
#ifndef SCL_CONCURRENT_SHARDED_ARRAY_DEFAULT_SHARD
#define SCL_CONCURRENT_SHARDED_ARRAY_DEFAULT_SHARD 256u
#endif

/* Forward-declare (scl_spinlock_t is defined in scl_concurrent_common.h) */
typedef struct scsa_shard {
  struct scsa_shard *next;
  unsigned char data[]; /* flexible array member: shard_len * elem_size bytes */
} scsa_shard_t;

typedef struct {
  scl_spinlock_t lock;
  scl_allocator_t *alloc;
  scsa_shard_t *head;
  scsa_shard_t *tail;
  size_t elem_size;
  size_t shard_len;
  atomic_size_t count;
  atomic_size_t shard_count;
} scl_concurrent_sharded_array_t;

scl_error_t scl_csa_init(scl_allocator_t *alloc,
                         scl_concurrent_sharded_array_t *csa, size_t elem_size,
                         size_t shard_len) SCL_WARN_UNUSED;
void scl_csa_destroy(scl_allocator_t *alloc,
                     scl_concurrent_sharded_array_t *csa);
scl_error_t scl_csa_append(scl_allocator_t *alloc,
                           scl_concurrent_sharded_array_t *csa,
                           const void *elem, size_t *out_index) SCL_WARN_UNUSED;
void *scl_csa_get(const scl_concurrent_sharded_array_t *csa, size_t index);
size_t scl_csa_count(const scl_concurrent_sharded_array_t *csa);

#endif