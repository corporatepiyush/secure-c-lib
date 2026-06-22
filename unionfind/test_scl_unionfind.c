#include "scl_unionfind.h"
#include "../testlib/scl_test.h"
#include <string.h>

static void test_find_union(scl_test_runner_t *tr)
{
    scl_test_group("find and union");
    scl_allocator_t *a = scl_allocator_default();
    scl_unionfind_t uf;
    scl_unionfind_init(a, &uf, 10);
    SCL_EXPECT_EQ_SZ(tr, scl_unionfind_sets(&uf), 10);
    scl_unionfind_union(&uf, 0, 1);
    scl_unionfind_union(&uf, 1, 2);
    scl_unionfind_union(&uf, 3, 4);
    SCL_EXPECT_TRUE(tr, scl_unionfind_connected(&uf, 0, 2));
    SCL_EXPECT_FALSE(tr, scl_unionfind_connected(&uf, 0, 3));
    SCL_EXPECT_EQ_SZ(tr, scl_unionfind_sets(&uf), 7);
    scl_unionfind_destroy(a, &uf);
}

static void test_bounds(scl_test_runner_t *tr)
{
    scl_test_group("bounds checks");
    scl_allocator_t *a = scl_allocator_default();
    scl_unionfind_t uf;
    scl_unionfind_init(a, &uf, 5);
    SCL_EXPECT_EQ_SZ(tr, scl_unionfind_find(&uf, 10), SIZE_MAX);
    SCL_EXPECT_FALSE(tr, scl_unionfind_connected(&uf, 10, 0));
    scl_unionfind_destroy(a, &uf);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_unionfind tests ===");
    test_find_union(&tr);
    test_bounds(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
