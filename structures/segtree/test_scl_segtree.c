#include "../../libs/testlib/scl_test.h"
#include "scl_segtree.h"

static void combine_min(void *out, const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    *(int*)out = va < vb ? va : vb;
}

static void test_segtree_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("SegTree: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_segtree_t tree;
    int data[] = {3, 2, -1, 6, 5, 4, -2, 3};
    scl_error_t err = scl_segtree_init(alloc, &tree, 8, sizeof(int), data, combine_min);
    SCL_EXPECT_OK(tr, err);
    scl_segtree_destroy(alloc, &tree);
}

static void test_segtree_query(scl_test_runner_t *tr) {
    scl_test_group("SegTree: query");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_segtree_t tree;
    int data[] = {3, 2, 1, 6, 5, 4, 2, 3};
    scl_segtree_init(alloc, &tree, 8, sizeof(int), data, combine_min);
    int result;
    scl_error_t err = scl_segtree_query(&tree, 2, 4, &result);
    SCL_EXPECT_OK(tr, err);
    scl_segtree_destroy(alloc, &tree);
}

static void test_segtree_update(scl_test_runner_t *tr) {
    scl_test_group("SegTree: update");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_segtree_t tree;
    int data[] = {3, 2, 1, 6, 5, 4, 2, 3};
    scl_segtree_init(alloc, &tree, 8, sizeof(int), data, combine_min);
    int newval = 0;
    scl_error_t err = scl_segtree_update(&tree, 3, &newval);
    SCL_EXPECT_OK(tr, err);
    scl_segtree_destroy(alloc, &tree);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_segtree_init_destroy(&tr);
    test_segtree_query(&tr);
    test_segtree_update(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
