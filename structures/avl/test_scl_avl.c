#include "../../libs/testlib/scl_test.h"
#include "scl_avl.h"

static int int_cmp(const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    return (va > vb) - (va < vb);
}

static void test_avl_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("AVL: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t tree;

    scl_error_t err = scl_avl_init(alloc, &tree, sizeof(int), int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&tree), 0);
    SCL_EXPECT_TRUE(tr, scl_avl_empty(&tree));

    scl_avl_destroy(alloc, &tree);
}

static void test_avl_insert_find(scl_test_runner_t *tr) {
    scl_test_group("AVL: insert and find");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t tree;
    scl_avl_init(alloc, &tree, sizeof(int), int_cmp);

    int val = 42;
    scl_error_t err = scl_avl_insert(alloc, &tree, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&tree), 1);

    int out;
    err = scl_avl_find(&tree, &val, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_avl_destroy(alloc, &tree);
}

static void test_avl_contains(scl_test_runner_t *tr) {
    scl_test_group("AVL: contains check");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t tree;
    scl_avl_init(alloc, &tree, sizeof(int), int_cmp);

    int val = 10;
    scl_avl_insert(alloc, &tree, &val);

    SCL_EXPECT_TRUE(tr, scl_avl_contains(&tree, &val));

    int other = 20;
    SCL_EXPECT_FALSE(tr, scl_avl_contains(&tree, &other));

    scl_avl_destroy(alloc, &tree);
}

static void test_avl_remove(scl_test_runner_t *tr) {
    scl_test_group("AVL: remove");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t tree;
    scl_avl_init(alloc, &tree, sizeof(int), int_cmp);

    int val = 5;
    scl_avl_insert(alloc, &tree, &val);
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&tree), 1);

    scl_error_t err = scl_avl_remove(alloc, &tree, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&tree), 0);
    SCL_EXPECT_FALSE(tr, scl_avl_contains(&tree, &val));

    scl_avl_destroy(alloc, &tree);
}

static void test_avl_balance(scl_test_runner_t *tr) {
    scl_test_group("AVL: balance maintenance");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t tree;
    scl_avl_init(alloc, &tree, sizeof(int), int_cmp);

    int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int i = 0; i < 9; i++) {
        scl_avl_insert(alloc, &tree, &values[i]);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&tree), 9);

    for (int i = 0; i < 9; i++) {
        int out;
        scl_avl_find(&tree, &values[i], &out);
        SCL_EXPECT_EQ_I(tr, out, values[i]);
    }

    scl_avl_destroy(alloc, &tree);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_avl_init_destroy(&tr);
    test_avl_insert_find(&tr);
    test_avl_contains(&tr);
    test_avl_remove(&tr);
    test_avl_balance(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
