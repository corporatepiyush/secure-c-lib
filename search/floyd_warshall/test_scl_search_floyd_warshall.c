#include "scl_search_floyd_warshall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

int main(void)
{
    printf("=== scl_search_floyd_warshall tests ===\n");

    {
        scl_edge_t edges[] = {
            {0, 1, 3}, {1, 2, 1}, {2, 0, 4}, {0, 2, 7}
        };
        int64_t dist[9];
        TEST("floyd-warshall basic");
        scl_error_t err = scl_search_floyd_warshall(3, edges, 4, dist);
        if (err == SCL_OK && dist[0*3+1] == 3 && dist[0*3+2] == 4 && dist[1*3+2] == 1) PASS();
        else FAIL("incorrect distances");
    }
    {
        scl_edge_t edges[] = {
            {0, 1, 1}, {1, 2, 1}, {2, 0, -3}
        };
        int64_t dist[9];
        TEST("floyd-warshall negative cycle");
        scl_error_t err = scl_search_floyd_warshall(3, edges, 3, dist);
        if (err == SCL_ERR_INVALID_STATE) PASS();
        else FAIL("expected INVALID_STATE");
    }
    {
        scl_edge_t edges[] = {{0, 1, 5}};
        int64_t dist[4];
        TEST("floyd-warshall 2 nodes");
        scl_error_t err = scl_search_floyd_warshall(2, edges, 1, dist);
        if (err == SCL_OK && dist[0*2+0] == 0 && dist[0*2+1] == 5 && dist[1*2+0] == INT64_MAX) PASS();
        else FAIL("incorrect");
    }
    {
        int64_t dist[1];
        TEST("floyd-warshall single node no edges");
        scl_error_t err = scl_search_floyd_warshall(1, NULL, 0, dist);
        if (err == SCL_OK && dist[0] == 0) PASS();
        else FAIL("expected 0");
    }
    {
        TEST("null edges");
        if (SCL_ERR_NULL_PTR == scl_search_floyd_warshall(2, NULL, 0, (int64_t*)(uintptr_t)1)) PASS();
        else FAIL("expected NULL_PTR");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
