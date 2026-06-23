#include "scl_test.h"
#include "scl_bst.h"

static int int_cmp(const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    return (va > vb) - (va < vb);
}

static void test_bst_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("BST: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t tree;

    scl_error_t err = scl_bst_init(alloc, &tree, sizeof(int), int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&tree), 0);
    SCL_EXPECT_TRUE(tr, scl_bst_empty(&tree));

    scl_bst_destroy(alloc, &tree);
}

static void test_bst_insert_find(scl_test_runner_t *tr) {
    scl_test_group("BST: insert and find");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t tree;
    scl_bst_init(alloc, &tree, sizeof(int), int_cmp);

    int val = 42;
    scl_error_t err = scl_bst_insert(alloc, &tree, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&tree), 1);

    int out;
    err = scl_bst_find(&tree, &val, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_bst_destroy(alloc, &tree);
}

static void test_bst_contains(scl_test_runner_t *tr) {
    scl_test_group("BST: contains check");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t tree;
    scl_bst_init(alloc, &tree, sizeof(int), int_cmp);

    int val = 10;
    scl_bst_insert(alloc, &tree, &val);

    SCL_EXPECT_TRUE(tr, scl_bst_contains(&tree, &val));

    int other = 20;
    SCL_EXPECT_FALSE(tr, scl_bst_contains(&tree, &other));

    scl_bst_destroy(alloc, &tree);
}

static void test_bst_remove(scl_test_runner_t *tr) {
    scl_test_group("BST: remove");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t tree;
    scl_bst_init(alloc, &tree, sizeof(int), int_cmp);

    int val = 5;
    scl_bst_insert(alloc, &tree, &val);
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&tree), 1);

    scl_error_t err = scl_bst_remove(alloc, &tree, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&tree), 0);
    SCL_EXPECT_FALSE(tr, scl_bst_contains(&tree, &val));

    scl_bst_destroy(alloc, &tree);
}

static void test_bst_min_max(scl_test_runner_t *tr) {
    scl_test_group("BST: min and max");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t tree;
    scl_bst_init(alloc, &tree, sizeof(int), int_cmp);

    int values[] = {5, 3, 7, 1, 9};
    for (int i = 0; i < 5; i++) {
        scl_bst_insert(alloc, &tree, &values[i]);
    }

    int minval, maxval;
    scl_bst_min(&tree, &minval);
    scl_bst_max(&tree, &maxval);

    SCL_EXPECT_EQ_I(tr, minval, 1);
    SCL_EXPECT_EQ_I(tr, maxval, 9);

    scl_bst_destroy(alloc, &tree);
}

static void test_bst_ordered_insert(scl_test_runner_t *tr) {
    scl_test_group("BST: ordered insert");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t tree;
    scl_bst_init(alloc, &tree, sizeof(int), int_cmp);

    int entries[] = {5, 3, 7, 1, 9, 2, 8};
    for (int i = 0; i < 7; i++) {
        scl_bst_insert(alloc, &tree, &entries[i]);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&tree), 7);

    for (int i = 0; i < 7; i++) {
        int out;
        scl_bst_find(&tree, &entries[i], &out);
        SCL_EXPECT_EQ_I(tr, out, entries[i]);
    }

    scl_bst_destroy(alloc, &tree);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_bst_init_destroy(&tr);
    test_bst_insert_find(&tr);
    test_bst_contains(&tr);
    test_bst_remove(&tr);
    test_bst_min_max(&tr);
    test_bst_ordered_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
