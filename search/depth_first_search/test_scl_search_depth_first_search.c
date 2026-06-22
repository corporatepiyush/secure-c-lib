#include "../../testlib/scl_test.h"
#include "scl_search_depth_first_search.h"
#include <stdlib.h>
#include <string.h>

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
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();

    {
        scl_graph_t g;
        make_graph(&g, 5);
        add_edge(&g, 0, 1, 1);
        add_edge(&g, 0, 2, 1);
        add_edge(&g, 1, 3, 1);
        add_edge(&g, 2, 4, 1);
        bool visited[5] = {false};
        scl_test_group("dfs");
        SCL_EXPECT_OK(&tr, scl_search_depth_first_search(a, &g, 0, visited));
        int count = 0;
        for (int i = 0; i < 5; i++) if (visited[i]) count++;
        SCL_EXPECT_EQ_I(&tr, 5, count);
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 4);
        add_edge(&g, 0, 1, 1);
        add_edge(&g, 1, 2, 1);
        bool visited[4] = {false};
        (void)scl_search_depth_first_search(a, &g, 0, visited);
        SCL_EXPECT_TRUE(&tr, visited[0] && visited[1] && visited[2] && !visited[3]);
        destroy_graph(&g);
    }
    {
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_depth_first_search(a, NULL, 0, (bool*)(uintptr_t)1));
    }
    {
        scl_graph_t g;
        make_graph(&g, 3);
        bool visited[3] = {false};
        SCL_EXPECT_OK(&tr, scl_search_depth_first_search(a, &g, 0, visited));
        SCL_EXPECT_TRUE(&tr, visited[0] && !visited[1]);
        destroy_graph(&g);
    }
    {
        scl_graph_t g;
        make_graph(&g, 5);
        add_edge(&g, 0, 1, 1);
        add_edge(&g, 1, 0, 1);
        bool visited[5] = {false};
        (void)scl_search_depth_first_search(a, &g, 0, visited);
        SCL_EXPECT_TRUE(&tr, visited[0] && visited[1] && !visited[2]);
        destroy_graph(&g);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
