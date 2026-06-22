#include "scl_search_breadth_first_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    printf("=== scl_search_breadth_first_search tests ===\n");

    {
        scl_graph_t g;
        make_graph(&g, 6);
        add_edge(&g, 0, 1, 1);
        add_edge(&g, 0, 2, 1);
        add_edge(&g, 1, 3, 1);
        add_edge(&g, 2, 4, 1);
        add_edge(&g, 3, 5, 1);
        bool visited[6] = {false};
        TEST("BFS traverses all");
        scl_error_t err = scl_search_breadth_first_search(&g, 0, visited);
        int cnt = 0;
        for (int i = 0; i < 6; i++) if (visited[i]) cnt++;
        if (err == SCL_OK && cnt == 6) PASS();
        else FAIL("expected all 6");
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 4);
        add_edge(&g, 0, 1, 1);
        add_edge(&g, 1, 2, 1);
        bool visited[4] = {false};
        TEST("BFS partial");
        (void)scl_search_breadth_first_search(&g, 0, visited);
        if (visited[0] && visited[1] && visited[2] && !visited[3]) PASS();
        else FAIL("expected 0,1,2");
        destroy_graph(&g);
    }
    {
        TEST("null");
        if (SCL_ERR_NULL_PTR == scl_search_breadth_first_search(NULL, 0, (bool*)(uintptr_t)1)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        scl_graph_t g;
        make_graph(&g, 3);
        bool visited[3] = {false};
        TEST("BFS single node");
        scl_error_t err = scl_search_breadth_first_search(&g, 0, visited);
        if (err == SCL_OK && visited[0] && !visited[1]) PASS();
        else FAIL("expected only 0");
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 4);
        add_edge(&g, 0, 1, 1);
        add_edge(&g, 1, 2, 1);
        add_edge(&g, 2, 0, 1);
        bool visited[4] = {false};
        TEST("BFS cycle");
        (void)scl_search_breadth_first_search(&g, 0, visited);
        if (visited[0] && visited[1] && visited[2] && !visited[3]) PASS();
        else FAIL("expected 0,1,2");
        destroy_graph(&g);
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
