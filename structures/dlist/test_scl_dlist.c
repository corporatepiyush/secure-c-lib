#include "../../libs/testlib/scl_test.h"
#include "scl_dlist.h"

static int int_cmp(const void *a, const void *b) {
    int va = *(int*)a, vb = *(int*)b;
    return (va > vb) - (va < vb);
}

static void test_dlist_push_front_back(scl_test_runner_t *tr) {
    scl_test_group("DList: push front and back");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));

    int val1 = 10, val2 = 20, val3 = 30;

    scl_error_t err = scl_dlist_push_front(alloc, &list, &val1);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_dlist_count(&list), 1);

    err = scl_dlist_push_back(alloc, &list, &val2);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_dlist_count(&list), 2);

    err = scl_dlist_push_front(alloc, &list, &val3);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_dlist_count(&list), 3);

    scl_dlist_destroy(alloc, &list);
}

static void test_dlist_pop_front_back(scl_test_runner_t *tr) {
    scl_test_group("DList: pop front and back");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));

    int values[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        scl_dlist_push_back(alloc, &list, &values[i]);
    }

    int out;
    scl_error_t err = scl_dlist_pop_front(alloc, &list, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 10);

    err = scl_dlist_pop_back(alloc, &list, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 30);
    SCL_EXPECT_EQ_SZ(tr, scl_dlist_count(&list), 1);

    scl_dlist_destroy(alloc, &list);
}

static void test_dlist_search(scl_test_runner_t *tr) {
    scl_test_group("DList: search");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));

    int values[] = {100, 200, 300};
    for (int i = 0; i < 3; i++) {
        scl_dlist_push_back(alloc, &list, &values[i]);
    }

    int search_val = 200;
    int found;
    scl_error_t err = scl_dlist_search(&list, &search_val, int_cmp, &found);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, found, 200);

    scl_dlist_destroy(alloc, &list);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    
    test_dlist_push_front_back(&tr);
    test_dlist_pop_front_back(&tr);
    test_dlist_search(&tr);
    
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
