#include "../../libs/testlib/scl_test.h"
#include "scl_rbtree.h"

static int int_cmp(const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    return (va > vb) - (va < vb);
}

static void test_rbtree_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("RBTree: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t tree;
    scl_error_t err = scl_rbtree_init(alloc, &tree, sizeof(int), int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 0);
    scl_rbtree_destroy(alloc, &tree);
}

static void test_rbtree_insert_find(scl_test_runner_t *tr) {
    scl_test_group("RBTree: insert and find");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t tree;
    scl_rbtree_init(alloc, &tree, sizeof(int), int_cmp);
    int val = 42;
    scl_error_t err = scl_rbtree_insert(alloc, &tree, &val);
    SCL_EXPECT_OK(tr, err);
    int out;
    err = scl_rbtree_find(&tree, &val, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);
    scl_rbtree_destroy(alloc, &tree);
}

static void test_rbtree_ordered(scl_test_runner_t *tr) {
    scl_test_group("RBTree: ordered insert");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t tree;
    scl_rbtree_init(alloc, &tree, sizeof(int), int_cmp);
    int entries[] = {5, 3, 7, 1, 9, 2, 8};
    for (int i = 0; i < 7; i++) {
        scl_rbtree_insert(alloc, &tree, &entries[i]);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 7);
    scl_rbtree_destroy(alloc, &tree);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_rbtree_init_destroy(&tr);
    test_rbtree_insert_find(&tr);
    test_rbtree_ordered(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
