#include "concurrent_dlist.h"
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
    scl_atomic_dlist_t list;
    assert(scl_atomic_dlist_init(scl_allocator_default(), &list, sizeof(int)) == SCL_OK);
    assert(scl_atomic_dlist_empty(&list));
    scl_atomic_dlist_destroy(scl_allocator_default(), &list);
    PASS();
}

static void test_push_pop(void)
{
    TEST("push_front/pop_front and push_back/pop_back");
    scl_atomic_dlist_t list;
    scl_atomic_dlist_init(scl_allocator_default(), &list, sizeof(int));
    for (int i = 0; i < 50; i++) {
        assert(scl_atomic_dlist_push_front(scl_allocator_default(), &list, &i) == SCL_OK);
        assert(scl_atomic_dlist_push_back(scl_allocator_default(), &list, &i) == SCL_OK);
    }
    assert(scl_atomic_dlist_count(&list) == 100);
    for (int i = 49; i >= 0; i--) {
        int v;
        assert(scl_atomic_dlist_pop_front(scl_allocator_default(), &list, &v) == SCL_OK && v == i);
        assert(scl_atomic_dlist_pop_back(scl_allocator_default(), &list, &v) == SCL_OK && v == i);
    }
    assert(scl_atomic_dlist_empty(&list));
    scl_atomic_dlist_destroy(scl_allocator_default(), &list);
    PASS();
}

static void test_insert_remove_at(void)
{
    TEST("insert_at and remove_at");
    scl_atomic_dlist_t list;
    scl_atomic_dlist_init(scl_allocator_default(), &list, sizeof(int));
    for (int i = 0; i < 5; i++) { int v = i * 10; scl_atomic_dlist_push_back(scl_allocator_default(), &list, &v); }
    int v = 99;
    assert(scl_atomic_dlist_insert_at(scl_allocator_default(), &list, 2, &v) == SCL_OK);
    assert(scl_atomic_dlist_count(&list) == 6);
    assert(scl_atomic_dlist_remove_at(scl_allocator_default(), &list, 2, &v) == SCL_OK && v == 99);
    assert(scl_atomic_dlist_count(&list) == 5);
    assert(scl_atomic_dlist_insert_at(scl_allocator_default(), &list, 5, &v) == SCL_OK);
    assert(scl_atomic_dlist_remove_at(scl_allocator_default(), &list, 5, &v) == SCL_OK && v == 99);
    scl_atomic_dlist_destroy(scl_allocator_default(), &list);
    PASS();
}

static void test_null_empty(void)
{
    TEST("null and empty checks");
    scl_atomic_dlist_t list;
    assert(scl_atomic_dlist_init(scl_allocator_default(), NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_dlist_init(scl_allocator_default(), &list, sizeof(int)) == SCL_OK);
    assert(scl_atomic_dlist_pop_front(scl_allocator_default(), &list, &(int){0}) == SCL_ERR_EMPTY);
    assert(scl_atomic_dlist_pop_back(scl_allocator_default(), &list, &(int){0}) == SCL_ERR_EMPTY);
    scl_atomic_dlist_destroy(scl_allocator_default(), &list);
    scl_atomic_dlist_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *push_front_thread(void *arg)
{
    scl_atomic_dlist_t *list = (scl_atomic_dlist_t *)arg;
    for (int i = 0; i < 50; i++) scl_atomic_dlist_push_front(scl_allocator_default(), list, &i);
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent push front 2 threads");
    scl_atomic_dlist_t list;
    scl_atomic_dlist_init(scl_allocator_default(), &list, sizeof(int));
    pthread_t t1, t2;
    pthread_create(&t1, NULL, push_front_thread, &list);
    pthread_create(&t2, NULL, push_front_thread, &list);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_dlist_count(&list) == 100);
    scl_atomic_dlist_destroy(scl_allocator_default(), &list);
    PASS();
}

int main(void)
{
    printf("=== scl_dlist tests ===\n");
    test_init_destroy();
    test_push_pop();
    test_insert_remove_at();
    test_null_empty();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
