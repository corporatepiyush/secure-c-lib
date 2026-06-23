#include "scl_test.h"
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

static void test_rbtree_remove(scl_test_runner_t *tr) {
    scl_test_group("RBTree: remove and rebalance");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t tree;
    scl_rbtree_init(alloc, &tree, sizeof(int), int_cmp);

    int entries[] = {5, 3, 7, 1, 9, 2, 8, 4, 6, 10};
    for (int i = 0; i < 10; i++)
        scl_rbtree_insert(alloc, &tree, &entries[i]);
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 10);

    /* Remove leaf (1 has no children) */
    int key = 1;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    SCL_EXPECT_FALSE(tr, scl_rbtree_contains(&tree, &key));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 9);

    /* Remove node with one child (3 has left child 2) */
    key = 3;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    SCL_EXPECT_FALSE(tr, scl_rbtree_contains(&tree, &key));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 8);

    /* Remove node with two children (5 is root) */
    key = 5;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    SCL_EXPECT_FALSE(tr, scl_rbtree_contains(&tree, &key));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 7);

    /* Remove non-existent */
    key = 99;
    SCL_EXPECT_TRUE(tr, scl_rbtree_remove(alloc, &tree, &key) != SCL_OK);

    /* Remove remaining entries */
    key = 2;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    key = 4;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    key = 6;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    key = 7;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    key = 8;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    key = 9;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));
    key = 10;
    SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &tree, &key));

    SCL_EXPECT_TRUE(tr, scl_rbtree_empty(&tree));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&tree), 0);

    scl_rbtree_destroy(alloc, &tree);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_rbtree_init_destroy(&tr);
    test_rbtree_insert_find(&tr);
    test_rbtree_ordered(&tr);
    test_rbtree_remove(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
