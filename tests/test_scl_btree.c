#include "scl_test.h"
#include "scl_btree.h"

static int int_cmp(const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    return (va > vb) - (va < vb);
}

static void test_btree_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("BTree: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_btree_t tree;

    scl_error_t err = scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 0);
    SCL_EXPECT_TRUE(tr, scl_btree_empty(&tree));

    scl_btree_destroy(alloc, &tree);
}

static void test_btree_insert_get(scl_test_runner_t *tr) {
    scl_test_group("BTree: insert and get");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_btree_t tree;
    scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);

    int key = 42, val = 100;
    scl_error_t err = scl_btree_insert(alloc, &tree, &key, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 1);

    int out;
    err = scl_btree_get(&tree, &key, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 100);

    scl_btree_destroy(alloc, &tree);
}

static void test_btree_contains(scl_test_runner_t *tr) {
    scl_test_group("BTree: contains check");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_btree_t tree;
    scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);

    int key = 10, val = 99;
    scl_btree_insert(alloc, &tree, &key, &val);

    SCL_EXPECT_TRUE(tr, scl_btree_contains(&tree, &key));

    int other_key = 20;
    SCL_EXPECT_FALSE(tr, scl_btree_contains(&tree, &other_key));

    scl_btree_destroy(alloc, &tree);
}


static void test_btree_ordered_insert(scl_test_runner_t *tr) {
    scl_test_group("BTree: ordered insert");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_btree_t tree;
    scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);

    int entries[] = {5, 3, 7, 1, 9, 2, 8};
    for (int i = 0; i < 7; i++) {
        int val = entries[i] * 10;
        scl_btree_insert(alloc, &tree, &entries[i], &val);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 7);

    for (int i = 0; i < 7; i++) {
        int out;
        scl_btree_get(&tree, &entries[i], &out);
        SCL_EXPECT_EQ_I(tr, out, entries[i] * 10);
    }

    scl_btree_destroy(alloc, &tree);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_btree_init_destroy(&tr);
    test_btree_insert_get(&tr);
    test_btree_contains(&tr);
    test_btree_ordered_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
