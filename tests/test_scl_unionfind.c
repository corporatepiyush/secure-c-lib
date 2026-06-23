#include "scl_test.h"
#include "scl_unionfind.h"

static void test_unionfind_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("UnionFind: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_unionfind_t uf;
    scl_error_t err = scl_unionfind_init(alloc, &uf, 10);
    SCL_EXPECT_OK(tr, err);
    scl_unionfind_destroy(alloc, &uf);
}

static void test_unionfind_find(scl_test_runner_t *tr) {
    scl_test_group("UnionFind: find");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_unionfind_t uf;
    scl_unionfind_init(alloc, &uf, 10);
    size_t root1 = scl_unionfind_find(&uf, 1);
    size_t root2 = scl_unionfind_find(&uf, 2);
    SCL_EXPECT_EQ_SZ(tr, root1, 1);
    SCL_EXPECT_EQ_SZ(tr, root2, 2);
    scl_unionfind_destroy(alloc, &uf);
}

static void test_unionfind_union(scl_test_runner_t *tr) {
    scl_test_group("UnionFind: union");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_unionfind_t uf;
    scl_unionfind_init(alloc, &uf, 10);
    scl_unionfind_union(&uf, 1, 2);
    size_t root1 = scl_unionfind_find(&uf, 1);
    size_t root2 = scl_unionfind_find(&uf, 2);
    SCL_EXPECT_EQ_SZ(tr, root1, root2);
    scl_unionfind_destroy(alloc, &uf);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_unionfind_init_destroy(&tr);
    test_unionfind_find(&tr);
    test_unionfind_union(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
