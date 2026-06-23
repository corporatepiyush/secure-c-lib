#include "scl_test.h"
#include "scl_skiplist.h"

static int int_cmp(const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    return (va > vb) - (va < vb);
}

static void test_skiplist_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("SkipList: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;

    scl_error_t err = scl_skiplist_init(alloc, &sl, sizeof(int), int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_skiplist_count(&sl), 0);
    SCL_EXPECT_TRUE(tr, scl_skiplist_empty(&sl));

    scl_skiplist_destroy(alloc, &sl);
}

static void test_skiplist_insert_find(scl_test_runner_t *tr) {
    scl_test_group("SkipList: insert and find");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;
    scl_skiplist_init(alloc, &sl, sizeof(int), int_cmp);

    int val = 42;
    scl_error_t err = scl_skiplist_insert(alloc, &sl, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_skiplist_count(&sl), 1);

    int out;
    err = scl_skiplist_find(&sl, &val, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_skiplist_destroy(alloc, &sl);
}

static void test_skiplist_contains(scl_test_runner_t *tr) {
    scl_test_group("SkipList: contains check");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;
    scl_skiplist_init(alloc, &sl, sizeof(int), int_cmp);

    int val = 10;
    scl_skiplist_insert(alloc, &sl, &val);

    SCL_EXPECT_TRUE(tr, scl_skiplist_contains(&sl, &val));

    int other = 20;
    SCL_EXPECT_FALSE(tr, scl_skiplist_contains(&sl, &other));

    scl_skiplist_destroy(alloc, &sl);
}

static void test_skiplist_remove(scl_test_runner_t *tr) {
    scl_test_group("SkipList: remove");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;
    scl_skiplist_init(alloc, &sl, sizeof(int), int_cmp);

    int val = 5;
    scl_skiplist_insert(alloc, &sl, &val);
    SCL_EXPECT_EQ_SZ(tr, scl_skiplist_count(&sl), 1);

    scl_error_t err = scl_skiplist_remove(alloc, &sl, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_skiplist_count(&sl), 0);
    SCL_EXPECT_FALSE(tr, scl_skiplist_contains(&sl, &val));

    scl_skiplist_destroy(alloc, &sl);
}

static void test_skiplist_ordered_insert(scl_test_runner_t *tr) {
    scl_test_group("SkipList: ordered insert");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;
    scl_skiplist_init(alloc, &sl, sizeof(int), int_cmp);

    int entries[] = {5, 3, 7, 1, 9, 2, 8};
    for (int i = 0; i < 7; i++) {
        scl_skiplist_insert(alloc, &sl, &entries[i]);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_skiplist_count(&sl), 7);

    for (int i = 0; i < 7; i++) {
        int out;
        scl_skiplist_find(&sl, &entries[i], &out);
        SCL_EXPECT_EQ_I(tr, out, entries[i]);
    }

    scl_skiplist_destroy(alloc, &sl);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_skiplist_init_destroy(&tr);
    test_skiplist_insert_find(&tr);
    test_skiplist_contains(&tr);
    test_skiplist_remove(&tr);
    test_skiplist_ordered_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
