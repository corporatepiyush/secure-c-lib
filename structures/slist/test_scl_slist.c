#include "../../libs/testlib/scl_test.h"
#include "scl_slist.h"

static void test_slist_push_pop(scl_test_runner_t *tr) {
    scl_test_group("SList: push and pop");
    
    scl_allocator_t *alloc = scl_allocator_default();
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    
    int values[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        scl_error_t err = scl_slist_push_front(alloc, &list, &values[i]);
        SCL_EXPECT_OK(tr, err);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_slist_count(&list), 5);

    int out;
    scl_error_t err = scl_slist_pop_front(alloc, &list, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 5);
    SCL_EXPECT_EQ_SZ(tr, scl_slist_count(&list), 4);
    
    scl_slist_destroy(alloc, &list);
}

static void test_slist_empty_checks(scl_test_runner_t *tr) {
    scl_test_group("SList: empty checks");
    
    scl_allocator_t *alloc = scl_allocator_default();
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    
    int out;
    scl_error_t err = scl_slist_pop_front(alloc, &list, &out);
    SCL_EXPECT_TRUE(tr, err == SCL_ERR_EMPTY);
    
    scl_slist_destroy(alloc, &list);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    
    test_slist_push_pop(&tr);
    test_slist_empty_checks(&tr);
    
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
