#include "scl_test.h"
#include "scl_fenwick.h"

static void add_int(void *out, const void *a, const void *b) {
    *(int*)out = *(int*)a + *(int*)b;
}

static void sub_int(void *out, const void *a, const void *b) {
    *(int*)out = *(int*)a - *(int*)b;
}

static void test_fenwick_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Fenwick: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_fenwick_t fw;
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    scl_error_t err = scl_fenwick_init(alloc, &fw, 8, sizeof(int), data, add_int, sub_int);
    SCL_EXPECT_OK(tr, err);
    scl_fenwick_destroy(alloc, &fw);
}

static void test_fenwick_prefix_query(scl_test_runner_t *tr) {
    scl_test_group("Fenwick: prefix query");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_fenwick_t fw;
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    scl_fenwick_init(alloc, &fw, 8, sizeof(int), data, add_int, sub_int);
    int result;
    scl_error_t err = scl_fenwick_prefix(&fw, 3, &result);
    SCL_EXPECT_OK(tr, err);
    scl_fenwick_destroy(alloc, &fw);
}

static void test_fenwick_update(scl_test_runner_t *tr) {
    scl_test_group("Fenwick: update");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_fenwick_t fw;
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    scl_fenwick_init(alloc, &fw, 8, sizeof(int), data, add_int, sub_int);
    int delta = 5;
    scl_error_t err = scl_fenwick_update(&fw, 2, &delta);
    SCL_EXPECT_OK(tr, err);
    scl_fenwick_destroy(alloc, &fw);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_fenwick_init_destroy(&tr);
    test_fenwick_prefix_query(&tr);
    test_fenwick_update(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
