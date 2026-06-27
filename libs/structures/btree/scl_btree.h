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

/* btree data structure. */

#ifndef SCL_BTREE_H
#define SCL_BTREE_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_BTREE_DEGREE 4
#define SCL_BTREE_MAX_KEYS (2 * SCL_BTREE_DEGREE - 1)
#define SCL_BTREE_MAX_CHILDREN (2 * SCL_BTREE_DEGREE)

/* B-Tree node with flat arrays backed by a single allocation.
 * Memory layout (single block):
 *   [scl_btree_node_t header] [keys: key_size * max_keys] [values: value_size * max_keys] [children: ptr * max_children] */
typedef struct scl_btree_node {
    size_t count;
    bool leaf;
} scl_btree_node_t;

static inline unsigned char *scl_btree_node_keys(scl_btree_node_t *n) {
    return (unsigned char *)(n + 1);
}
static inline unsigned char *scl_btree_node_vals(scl_btree_node_t *n, size_t ksz, size_t maxk) {
    return (unsigned char *)(n + 1) + ksz * maxk;
}
static inline scl_btree_node_t **scl_btree_node_children(scl_btree_node_t *n, size_t ksz, size_t vsz, size_t maxk) {
    return (scl_btree_node_t **)((unsigned char *)(n + 1) + ksz * maxk + vsz * maxk);
}

typedef struct {
    scl_btree_node_t *root;
    size_t key_size;
    size_t value_size;
    size_t count;
    scl_cmp_func_t cmp;
    int t;
} scl_btree_t;

scl_error_t scl_btree_init(scl_allocator_t *alloc, scl_btree_t *tree, size_t key_size, size_t value_size,
                           int degree, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_btree_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_btree_t *SCL_RESTRICT tree);
scl_error_t scl_btree_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_btree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key, const void *SCL_RESTRICT value) SCL_WARN_UNUSED;
scl_error_t scl_btree_get(const scl_btree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key, void *SCL_RESTRICT out_value) SCL_WARN_UNUSED;
scl_error_t scl_btree_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_btree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool        scl_btree_contains(const scl_btree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key);
SCL_PURE size_t      scl_btree_count(const scl_btree_t *SCL_RESTRICT tree);
SCL_PURE bool        scl_btree_empty(const scl_btree_t *SCL_RESTRICT tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
