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

/* Floyd-Warshall APSP. O(V^3). DP over intermediate vertices. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_floyd_warshall.h"
#include <limits.h>
#include <stdlib.h>

scl_error_t scl_search_floyd_warshall(int n, const scl_edge_t * edges, size_t ecount, int64_t * dist)
{
    if (scl_unlikely(edges == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(dist == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(n <= 0)) return SCL_ERR_EMPTY;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dist[i * n + j] = (i == j) ? 0 : INT64_MAX;
        }
    }
    for (size_t e = 0; e < ecount; e++) {
        int f = (int)edges[e].from;
        int t = (int)edges[e].to;
        if (f >= 0 && f < n && t >= 0 && t < n)
            dist[f * n + t] = (int64_t)edges[e].weight;
    }

    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            if (dist[i * n + k] == INT64_MAX) continue;
            for (int j = 0; j < n; j++) {
                if (dist[k * n + j] == INT64_MAX) continue;
                int64_t nd = dist[i * n + k] + dist[k * n + j];
                if (nd < dist[i * n + j])
                    dist[i * n + j] = nd;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        if (dist[i * n + i] < 0)
            return SCL_ERR_INVALID_STATE;
    }
    return SCL_OK;
}
