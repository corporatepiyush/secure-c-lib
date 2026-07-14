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

/* A* graph search. f = g + h heuristic. Optimal with admissible heuristic.
 * Binary-heap open set. */

#include "scl_a_star.h"
#include "scl_limits.h"
#include "scl_math.h"
#include "scl_stdlib.h"
#include "scl_string.h"

typedef struct {
  int x, y;
  int g, f;
} a_star_node_t;

static int heuristic(int x1, int y1, int x2, int y2) {
  return abs(x1 - x2) + abs(y1 - y2);
}

/* Binary min-heap (by f) for the open set. The previous open set was a
 * fixed 4096-entry array searched linearly for the minimum — O(N) per pop, and
 * worse, once it filled it SILENTLY DROPPED new nodes, so A* could return
 * "not found" on a solvable map. A heap sized to the relaxation bound removes
 * both problems and matches the documented "binary-heap open set". */
static void as_sift_up(a_star_node_t *h, size_t i) {
  while (i > 0) {
    size_t p = (i - 1) / 2;
    if (h[p].f <= h[i].f)
      break;
    a_star_node_t t = h[p];
    h[p] = h[i];
    h[i] = t;
    i = p;
  }
}

static void as_sift_down(a_star_node_t *h, size_t n, size_t i) {
  for (;;) {
    size_t l = 2 * i + 1, r = 2 * i + 2, m = i;
    if (l < n && h[l].f < h[m].f)
      m = l;
    if (r < n && h[r].f < h[m].f)
      m = r;
    if (m == i)
      break;
    a_star_node_t t = h[m];
    h[m] = h[i];
    h[i] = t;
    i = m;
  }
}

scl_error_t scl_search_a_star(scl_allocator_t *alloc, int sx, int sy, int gx,
                              int gy, int **SCL_RESTRICT grid, int w, int h,
                              int *px, int *py, size_t *plen, size_t maxplen) {
  if (scl_unlikely(grid == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(px == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(py == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(plen == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(w <= 0 || h <= 0))
    return SCL_ERR_EMPTY;
  if (scl_unlikely(sx < 0 || sx >= w || sy < 0 || sy >= h))
    return SCL_ERR_INVALID_INDEX;
  if (scl_unlikely(gx < 0 || gx >= w || gy < 0 || gy >= h))
    return SCL_ERR_INVALID_INDEX;

  if (grid[sy][sx] != 0 || grid[gy][gx] != 0)
    return SCL_ERR_INVALID_ARG;

  /* (size_t)(w * h) computes w*h in int first — undefined overflow for large
   * grids and an under-sized allocation. Multiply as size_t with a guard. */
  size_t W = (size_t)w, H = (size_t)h;
  size_t cell_count, ibytes, cap, obytes;
  if (scl_unlikely(scl_mul_overflow(W, H, &cell_count)))
    return SCL_ERR_SIZE_OVERFLOW;
  if (scl_unlikely(scl_mul_overflow(cell_count, sizeof(int), &ibytes)))
    return SCL_ERR_SIZE_OVERFLOW;
  /* Open-set bound: at most 4 relaxations (neighbours) per cell, plus start. */
  if (scl_unlikely(scl_mul_overflow(cell_count, 4, &cap) ||
                   scl_add_overflow(cap, 1, &cap)))
    return SCL_ERR_SIZE_OVERFLOW;
  if (scl_unlikely(scl_mul_overflow(cap, sizeof(a_star_node_t), &obytes)))
    return SCL_ERR_SIZE_OVERFLOW;

  int *g_score = (int *)scl_alloc(alloc, ibytes, alignof(max_align_t));
  int *f_score = (int *)scl_alloc(alloc, ibytes, alignof(max_align_t));
  int *came_from_x = (int *)scl_alloc(alloc, ibytes, alignof(max_align_t));
  int *came_from_y = (int *)scl_alloc(alloc, ibytes, alignof(max_align_t));
  bool *closed =
      (bool *)scl_calloc(alloc, cell_count, sizeof(bool), alignof(max_align_t));
  a_star_node_t *open =
      (a_star_node_t *)scl_alloc(alloc, obytes, alignof(max_align_t));

  if (!g_score || !f_score || !came_from_x || !came_from_y || !closed ||
      !open) {
    scl_free(alloc, g_score);
    scl_free(alloc, f_score);
    scl_free(alloc, came_from_x);
    scl_free(alloc, came_from_y);
    scl_free(alloc, closed);
    scl_free(alloc, open);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < cell_count; i++) {
    g_score[i] = INT_MAX;
    f_score[i] = INT_MAX;
    came_from_x[i] = -1;
    came_from_y[i] = -1;
  }

  size_t sidx = (size_t)sy * W + (size_t)sx;
  g_score[sidx] = 0;
  f_score[sidx] = heuristic(sx, sy, gx, gy);

  size_t hn = 0;
  open[hn].x = sx;
  open[hn].y = sy;
  open[hn].g = 0;
  open[hn].f = f_score[sidx];
  hn++;

  const int dx[] = {1, -1, 0, 0};
  const int dy[] = {0, 0, 1, -1};

  scl_error_t result = SCL_ERR_NOT_FOUND;

  while (hn > 0) {
    a_star_node_t cur = open[0];
    open[0] = open[--hn];
    as_sift_down(open, hn, 0);

    size_t cidx = (size_t)cur.y * W + (size_t)cur.x;
    if (closed[cidx])
      continue;
    closed[cidx] = true;

    if (cur.x == gx && cur.y == gy) {
      size_t path_len = 0;
      int cx = gx, cy = gy;
      while (cx != -1 && cy != -1 && path_len < maxplen) {
        px[path_len] = cx;
        py[path_len] = cy;
        path_len++;
        size_t nidx = (size_t)cy * W + (size_t)cx;
        int nx = came_from_x[nidx];
        int ny = came_from_y[nidx];
        cx = nx;
        cy = ny;
      }
      for (size_t i = 0; i < path_len / 2; i++) {
        int tx = px[i];
        int ty = py[i];
        px[i] = px[path_len - 1 - i];
        py[i] = py[path_len - 1 - i];
        px[path_len - 1 - i] = tx;
        py[path_len - 1 - i] = ty;
      }
      *plen = path_len;
      result = SCL_OK;
      break;
    }

    for (int d = 0; d < 4; d++) {
      int nx = cur.x + dx[d];
      int ny = cur.y + dy[d];
      if (nx < 0 || nx >= w || ny < 0 || ny >= h)
        continue;
      if (grid[ny][nx] != 0)
        continue;

      size_t nidx = (size_t)ny * W + (size_t)nx;
      if (closed[nidx])
        continue;

      int tentative_g = cur.g + 1;
      if (tentative_g < g_score[nidx]) {
        came_from_x[nidx] = cur.x;
        came_from_y[nidx] = cur.y;
        g_score[nidx] = tentative_g;
        int f = tentative_g + heuristic(nx, ny, gx, gy);
        f_score[nidx] = f;
        if (scl_likely(hn < cap)) { /* bound holds; defensive */
          open[hn].x = nx;
          open[hn].y = ny;
          open[hn].g = tentative_g;
          open[hn].f = f;
          as_sift_up(open, hn);
          hn++;
        }
      }
    }
  }

  scl_free(alloc, g_score);
  scl_free(alloc, f_score);
  scl_free(alloc, came_from_x);
  scl_free(alloc, came_from_y);
  scl_free(alloc, closed);
  scl_free(alloc, open);
  return result;
}
