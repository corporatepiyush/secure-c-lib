#include "scl_search_bellman_ford.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static scl_error_t make_graph(scl_graph_t *g, size_t n)
{
    g->adj = (scl_adj_node_t **)calloc(n, sizeof(scl_adj_node_t *));
    if (!g->adj) return SCL_ERR_OUT_OF_MEMORY;
    g->vertex_count = n;
    g->edge_count = 0;
    return SCL_OK;
}

static void destroy_graph(scl_graph_t *g)
{
    for (size_t i = 0; i < g->vertex_count; i++) {
        scl_adj_node_t *node = g->adj[i];
        while (node) {
            scl_adj_node_t *tmp = node;
            node = node->next;
            free(tmp);
        }
    }
    free(g->adj);
    g->adj = NULL;
    g->vertex_count = 0;
    g->edge_count = 0;
}

static scl_error_t add_edge(scl_graph_t *g, size_t from, size_t to, int weight)
{
    scl_adj_node_t *node = (scl_adj_node_t *)malloc(sizeof(scl_adj_node_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->to = to;
    node->weight = weight;
    node->next = g->adj[from];
    g->adj[from] = node;
    g->edge_count++;
    return SCL_OK;
}

int main(void)
{
    printf("=== scl_search_bellman_ford tests ===\n");

    {
        scl_graph_t g;
        make_graph(&g, 5);
        add_edge(&g, 0, 1, 6);
        add_edge(&g, 0, 3, 7);
        add_edge(&g, 1, 2, 5);
        add_edge(&g, 1, 3, 8);
        add_edge(&g, 1, 4, -4);
        add_edge(&g, 2, 1, -2);
        add_edge(&g, 3, 2, -3);
        add_edge(&g, 3, 4, 9);
        add_edge(&g, 4, 0, 2);
        add_edge(&g, 4, 2, 7);
        int64_t dist[5];
        int prev[5];
        TEST("bellman-ford distances");
        scl_error_t err = scl_search_bellman_ford(&g, 0, dist, prev);
        if (err == SCL_OK && dist[0] == 0 && dist[1] == 2 && dist[2] == 4 && dist[3] == 7 && dist[4] == -2) PASS();
        else FAIL("incorrect distances");
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 3);
        add_edge(&g, 0, 1, -1);
        add_edge(&g, 1, 2, -1);
        add_edge(&g, 2, 0, -1);
        int64_t dist[3];
        int prev[3];
        TEST("bellman-ford negative cycle");
        scl_error_t err = scl_search_bellman_ford(&g, 0, dist, prev);
        if (err == SCL_ERR_INVALID_STATE) PASS();
        else FAIL("expected INVALID_STATE");
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 3);
        add_edge(&g, 0, 1, 5);
        int64_t dist[3];
        int prev[3];
        TEST("bellman-ford unreachable");
        scl_error_t err = scl_search_bellman_ford(&g, 0, dist, prev);
        if (err == SCL_OK && dist[0] == 0 && dist[1] == 5 && dist[2] == INT64_MAX) PASS();
        else FAIL("expected unreachable");
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 1);
        int64_t dist[1];
        int prev[1];
        TEST("bellman-ford single node");
        scl_error_t err = scl_search_bellman_ford(&g, 0, dist, prev);
        if (err == SCL_OK && dist[0] == 0) PASS();
        else FAIL("expected 0");
        destroy_graph(&g);
    }
    {
        TEST("null");
        if (SCL_ERR_NULL_PTR == scl_search_bellman_ford(NULL, 0, (int64_t*)(uintptr_t)1, (int*)(uintptr_t)1)) PASS();
        else FAIL("expected NULL_PTR");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
