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

    /* Index with size_t: `i * n + j` computed in int overflows (UB, then an
     * out-of-bounds access) once n exceeds ~46340. */
    size_t sn = (size_t)n;
    for (size_t i = 0; i < sn; i++)
        for (size_t j = 0; j < sn; j++)
            dist[i * sn + j] = (i == j) ? 0 : INT64_MAX;

    for (size_t e = 0; e < ecount; e++) {
        size_t f = edges[e].from;
        size_t t = edges[e].to;
        if (f < sn && t < sn)
            dist[f * sn + t] = (int64_t)edges[e].weight;
    }

    for (size_t k = 0; k < sn; k++) {
        for (size_t i = 0; i < sn; i++) {
            int64_t dik = dist[i * sn + k];
            if (dik == INT64_MAX) continue;
            for (size_t j = 0; j < sn; j++) {
                int64_t dkj = dist[k * sn + j];
                if (dkj == INT64_MAX) continue;
                /* Saturating add: two finite-but-large distances can overflow
                 * int64 (signed overflow is UB). Skip such relaxations. */
                int64_t nd;
                if (__builtin_add_overflow(dik, dkj, &nd)) continue;
                if (nd < dist[i * sn + j])
                    dist[i * sn + j] = nd;
            }
        }
    }

    for (size_t i = 0; i < sn; i++)
        if (dist[i * sn + i] < 0)
            return SCL_ERR_INVALID_STATE;
    return SCL_OK;
}
