#include "../../testlib/scl_test.h"
#include "scl_search_floyd_warshall.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        scl_edge_t edges[] = {
            {0, 1, 3}, {1, 2, 1}, {2, 0, 4}, {0, 2, 7}
        };
        int64_t dist[9];
        scl_test_group("floyd_warshall");
        SCL_EXPECT_OK(&tr, scl_search_floyd_warshall(3, edges, 4, dist));
        SCL_EXPECT_EQ_I(&tr, 3, dist[0*3+1]);
        SCL_EXPECT_EQ_I(&tr, 4, dist[0*3+2]);
        SCL_EXPECT_EQ_I(&tr, 1, dist[1*3+2]);
    }
    {
        scl_edge_t edges[] = {
            {0, 1, 1}, {1, 2, 1}, {2, 0, -3}
        };
        int64_t dist[9];
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_INVALID_STATE, scl_search_floyd_warshall(3, edges, 3, dist));
    }
    {
        scl_edge_t edges[] = {{0, 1, 5}};
        int64_t dist[4];
        SCL_EXPECT_OK(&tr, scl_search_floyd_warshall(2, edges, 1, dist));
        SCL_EXPECT_EQ_I(&tr, 0, dist[0*2+0]);
        SCL_EXPECT_EQ_I(&tr, 5, dist[0*2+1]);
        SCL_EXPECT_EQ_I(&tr, INT64_MAX, dist[1*2+0]);
    }
    {
        int64_t dist[1];
        SCL_EXPECT_OK(&tr, scl_search_floyd_warshall(1, NULL, 0, dist));
        SCL_EXPECT_EQ_I(&tr, 0, dist[0]);
    }
    {
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_floyd_warshall(2, NULL, 0, (int64_t*)(uintptr_t)1));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
