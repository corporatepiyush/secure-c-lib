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

#ifndef SCL_GRAPH_TYPES_H
#define SCL_GRAPH_TYPES_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Full flat-array struct definition (not just opaque) so scl_graph_t
 * can embed scl_array_t nodes and edges by value. */
#include "scl_array_types.h"

#ifndef SCL_GRAPH_TYPES_DEFINED
#define SCL_GRAPH_TYPES_DEFINED

/* Sentinel "no edge" / end-of-adjacency-chain index. */
#define SCL_GRAPH_NIL ((size_t)-1)

/* A vertex record: index of its first outgoing edge (or SCL_GRAPH_NIL). */
typedef struct {
  size_t head;
} scl_graph_node_t;

/* An edge record: destination, weight, and the next edge index. */
typedef struct {
  size_t to;
  int weight;
  size_t next;
} scl_graph_edge_t;

/* Graph stores nodes and edges in two scl_array containers. */
typedef struct {
  scl_array_t nodes;
  scl_array_t edges;
  size_t vertex_count;
  size_t edge_count;
  size_t free_head;
} scl_graph_t;

typedef struct {
  size_t from;
  size_t to;
  int weight;
} scl_edge_t;

#endif

#endif