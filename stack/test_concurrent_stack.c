#include "concurrent_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_atomic_stack_t stack;
    assert(scl_atomic_stack_init(scl_allocator_default(), &stack, sizeof(int)) == SCL_OK);
    assert(scl_atomic_stack_empty(&stack));
    scl_atomic_stack_destroy(scl_allocator_default(), &stack);
    PASS();
}

static void test_push_pop_lifo(void)
{
    TEST("push/pop LIFO ordering");
    scl_atomic_stack_t stack;
    scl_atomic_stack_init(scl_allocator_default(), &stack, sizeof(int));
    for (int i = 0; i < 100; i++)
        assert(scl_atomic_stack_push(scl_allocator_default(), &stack, &i) == SCL_OK);
    assert(scl_atomic_stack_count(&stack) == 100);
    for (int i = 99; i >= 0; i--) {
        int v;
        assert(scl_atomic_stack_pop(scl_allocator_default(), &stack, &v) == SCL_OK);
        assert(v == i);
    }
    assert(scl_atomic_stack_empty(&stack));
    assert(scl_atomic_stack_pop(scl_allocator_default(), &stack, &(int){0}) == SCL_ERR_EMPTY);
    scl_atomic_stack_destroy(scl_allocator_default(), &stack);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_stack_init(scl_allocator_default(), NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_stack_push(scl_allocator_default(), NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_stack_pop(scl_allocator_default(), NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    scl_atomic_stack_destroy(scl_allocator_default(), NULL);
    PASS();
}

typedef struct {
    scl_atomic_stack_t *stack;
    int start;
    int count;
} thread_arg_t;

static void *push_thread(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    for (int i = 0; i < ta->count; i++) {
        int v = ta->start + i;
        scl_atomic_stack_push(scl_allocator_default(), ta->stack, &v);
    }
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent Treiber push 2 threads x 50");
    scl_atomic_stack_t stack;
    scl_atomic_stack_init(scl_allocator_default(), &stack, sizeof(int));
    pthread_t t1, t2;
    thread_arg_t a1 = {&stack, 0, 50};
    thread_arg_t a2 = {&stack, 50, 50};
    pthread_create(&t1, NULL, push_thread, &a1);
    pthread_create(&t2, NULL, push_thread, &a2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_stack_count(&stack) == 100);
    int found[100] = {0};
    while (!scl_atomic_stack_empty(&stack)) {
        int v;
        scl_atomic_stack_pop(scl_allocator_default(), &stack, &v);
        assert(v >= 0 && v < 100);
        found[v] = 1;
    }
    for (int i = 0; i < 100; i++) assert(found[i]);
    scl_atomic_stack_destroy(scl_allocator_default(), &stack);
    PASS();
}

int main(void)
{
    printf("=== scl_stack tests ===\n");
    test_init_destroy();
    test_push_pop_lifo();
    test_null();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
