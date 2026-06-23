#include "scl_test.h"
#include "scl_stack.h"

static void test_stack_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Stack: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_stack_t stack;

    scl_error_t err = scl_stack_init(alloc, &stack, sizeof(int), 10);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_stack_count(&stack), 0);
    SCL_EXPECT_TRUE(tr, scl_stack_empty(&stack));

    scl_stack_destroy(alloc, &stack);
}

static void test_stack_push_pop(scl_test_runner_t *tr) {
    scl_test_group("Stack: push and pop");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_stack_t stack;
    scl_stack_init(alloc, &stack, sizeof(int), 10);

    int values[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        scl_error_t err = scl_stack_push(alloc, &stack, &values[i]);
        SCL_EXPECT_OK(tr, err);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_stack_count(&stack), 3);

    int out;
    scl_error_t err = scl_stack_pop(&stack, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 30);
    SCL_EXPECT_EQ_SZ(tr, scl_stack_count(&stack), 2);

    scl_stack_destroy(alloc, &stack);
}

static void test_stack_peek(scl_test_runner_t *tr) {
    scl_test_group("Stack: peek without pop");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_stack_t stack;
    scl_stack_init(alloc, &stack, sizeof(int), 10);

    int val = 42;
    scl_stack_push(alloc, &stack, &val);

    int out;
    scl_error_t err = scl_stack_peek(&stack, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_EQ_SZ(tr, scl_stack_count(&stack), 1);

    scl_stack_destroy(alloc, &stack);
}

static void test_stack_empty_checks(scl_test_runner_t *tr) {
    scl_test_group("Stack: empty checks");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_stack_t stack;
    scl_stack_init(alloc, &stack, sizeof(int), 10);

    int out;
    scl_error_t err = scl_stack_pop(&stack, &out);
    SCL_EXPECT_TRUE(tr, err == SCL_ERR_EMPTY);

    scl_stack_destroy(alloc, &stack);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_stack_init_destroy(&tr);
    test_stack_push_pop(&tr);
    test_stack_peek(&tr);
    test_stack_empty_checks(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
