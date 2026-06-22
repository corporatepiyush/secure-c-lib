#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_a_star.h"
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#define A_STAR_MAX_OPEN 4096

typedef struct {
    int x, y;
    int g, f;
} a_star_node_t;

static int heuristic(int x1, int y1, int x2, int y2)
{
    return abs(x1 - x2) + abs(y1 - y2);
}

scl_error_t scl_search_a_star(int sx, int sy, int gx, int gy, int **restrict grid, int w, int h, int *restrict px, int *restrict py, size_t *restrict plen, size_t maxplen)
{
    if (__builtin_expect(grid == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(px == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(py == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(plen == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(w <= 0 || h <= 0, 0)) return SCL_ERR_EMPTY;
    if (__builtin_expect(sx < 0 || sx >= w || sy < 0 || sy >= h, 0)) return SCL_ERR_INVALID_INDEX;
    if (__builtin_expect(gx < 0 || gx >= w || gy < 0 || gy >= h, 0)) return SCL_ERR_INVALID_INDEX;

    if (grid[sy][sx] != 0 || grid[gy][gx] != 0) return SCL_ERR_INVALID_ARG;

    int *g_score = (int *)calloc((size_t)(w * h), sizeof(int));
    int *f_score = (int *)malloc((size_t)(w * h) * sizeof(int));
    int *came_from_x = (int *)malloc((size_t)(w * h) * sizeof(int));
    int *came_from_y = (int *)malloc((size_t)(w * h) * sizeof(int));
    bool *closed = (bool *)calloc((size_t)(w * h), sizeof(bool));
    a_star_node_t *open = (a_star_node_t *)malloc((size_t)A_STAR_MAX_OPEN * sizeof(a_star_node_t));

    if (!g_score || !f_score || !came_from_x || !came_from_y || !closed || !open) {
        free(g_score); free(f_score); free(came_from_x); free(came_from_y);
        free(closed); free(open);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (int i = 0; i < w * h; i++) {
        g_score[i] = INT_MAX;
        f_score[i] = INT_MAX;
        came_from_x[i] = -1;
        came_from_y[i] = -1;
    }

    g_score[sy * w + sx] = 0;
    f_score[sy * w + sx] = heuristic(sx, sy, gx, gy);

    int open_count = 1;
    open[0].x = sx;
    open[0].y = sy;
    open[0].g = 0;
    open[0].f = f_score[sy * w + sx];

    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    scl_error_t result = SCL_ERR_NOT_FOUND;

    while (open_count > 0) {
        int best_idx = 0;
        for (int i = 1; i < open_count; i++) {
            if (open[i].f < open[best_idx].f)
                best_idx = i;
        }
        a_star_node_t cur = open[best_idx];
        open[best_idx] = open[--open_count];

        int cidx = cur.y * w + cur.x;
        if (closed[cidx]) continue;
        closed[cidx] = true;

        if (cur.x == gx && cur.y == gy) {
            size_t path_len = 0;
            int cx = gx, cy = gy;
            while (cx != -1 && cy != -1 && path_len < maxplen) {
                px[path_len] = cx;
                py[path_len] = cy;
                path_len++;
                int nidx = cy * w + cx;
                int nx = came_from_x[nidx];
                int ny = came_from_y[nidx];
                cx = nx; cy = ny;
            }
            for (size_t i = 0; i < path_len / 2; i++) {
                int tx = px[i]; int ty = py[i];
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
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (grid[ny][nx] != 0) continue;

            int nidx = ny * w + nx;
            if (closed[nidx]) continue;

            int tentative_g = cur.g + 1;
            if (tentative_g < g_score[nidx]) {
                came_from_x[nidx] = cur.x;
                came_from_y[nidx] = cur.y;
                g_score[nidx] = tentative_g;
                int f = tentative_g + heuristic(nx, ny, gx, gy);
                f_score[nidx] = f;
                if (open_count < A_STAR_MAX_OPEN) {
                    open[open_count].x = nx;
                    open[open_count].y = ny;
                    open[open_count].g = tentative_g;
                    open[open_count].f = f;
                    open_count++;
                }
            }
        }
    }

    free(g_score); free(f_score);
    free(came_from_x); free(came_from_y);
    free(closed); free(open);
    return result;
}
