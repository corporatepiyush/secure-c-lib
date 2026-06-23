#include "../../libs/testlib/scl_test.h"
#include "scl_array.h"
#include <stdlib.h>
#include <string.h>

static void test_array_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Array: init and destroy");
    
    scl_allocator_t *alloc = scl_allocator_default();
    scl_array_t arr;
    
    scl_error_t err = scl_array_init(alloc, &arr, sizeof(int), 10);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_array_count(&arr), 0);
    SCL_EXPECT_EQ_SZ(tr, scl_array_capacity(&arr), 10);
    
    scl_array_destroy(alloc, &arr);
}

static void test_array_push_pop(scl_test_runner_t *tr) {
    scl_test_group("Array: push and pop");
    
    scl_allocator_t *alloc = scl_allocator_default();
    scl_array_t arr;
    scl_array_init(alloc, &arr, sizeof(int), 10);
    
    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        scl_error_t err = scl_array_push(alloc, &arr, &values[i]);
        SCL_EXPECT_OK(tr, err);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_array_count(&arr), 5);

    int last;
    scl_error_t err = scl_array_pop(&arr, &last);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, last, 50);
    SCL_EXPECT_EQ_SZ(tr, scl_array_count(&arr), 4);
    
    scl_array_destroy(alloc, &arr);
}

static void test_array_get_set(scl_test_runner_t *tr) {
    scl_test_group("Array: get and set");
    
    scl_allocator_t *alloc = scl_allocator_default();
    scl_array_t arr;
    scl_array_init(alloc, &arr, sizeof(int), 10);
    
    int values[] = {100, 200, 300};
    for (int i = 0; i < 3; i++) {
        scl_array_push(alloc, &arr, &values[i]);
    }
    
    int val;
    scl_array_get(&arr, 1, &val);
    SCL_EXPECT_EQ_I(tr, val, 200);
    
    int new_val = 250;
    scl_array_set(&arr, 1, &new_val);
    scl_array_get(&arr, 1, &val);
    SCL_EXPECT_EQ_I(tr, val, 250);
    
    scl_array_destroy(alloc, &arr);
}

static void test_array_null_checks(scl_test_runner_t *tr) {
    scl_test_group("Array: NULL checks");
    
    scl_allocator_t *alloc = scl_allocator_default();
    
    scl_error_t err = scl_array_init(alloc, NULL, sizeof(int), 10);
    SCL_EXPECT_TRUE(tr, err == SCL_ERR_NULL_PTR);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    
    test_array_init_destroy(&tr);
    test_array_push_pop(&tr);
    test_array_get_set(&tr);
    test_array_null_checks(&tr);
    
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
