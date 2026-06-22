#include "concurrent_slist.h"
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
    scl_concurrent_slist_t list;
    assert(scl_concurrent_slist_init(&list, sizeof(int)) == SCL_OK);
    assert(scl_concurrent_slist_empty(&list));
    scl_concurrent_slist_destroy(&list);
    PASS();
}

static void test_push_pop_front(void)
{
    TEST("push_front and pop_front");
    scl_concurrent_slist_t list;
    scl_concurrent_slist_init(&list, sizeof(int));
    for (int i = 0; i < 100; i++)
        assert(scl_concurrent_slist_push_front(&list, &i) == SCL_OK);
    assert(scl_concurrent_slist_count(&list) == 100);
    for (int i = 99; i >= 0; i--) {
        int v;
        assert(scl_concurrent_slist_pop_front(&list, &v) == SCL_OK);
        assert(v == i);
    }
    assert(scl_concurrent_slist_empty(&list));
    assert(scl_concurrent_slist_pop_front(&list, &(int){0}) == SCL_ERR_EMPTY);
    scl_concurrent_slist_destroy(&list);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_slist_init(NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    assert(scl_concurrent_slist_push_front(NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_concurrent_slist_pop_front(NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    scl_concurrent_slist_destroy(NULL);
    PASS();
}

typedef struct {
    scl_concurrent_slist_t *list;
    int start;
    int count;
} thread_arg_t;

static void *push_thread(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    for (int i = 0; i < ta->count; i++) {
        int v = ta->start + i;
        scl_concurrent_slist_push_front(ta->list, &v);
    }
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent push 2 threads x 50");
    scl_concurrent_slist_t list;
    scl_concurrent_slist_init(&list, sizeof(int));
    pthread_t t1, t2;
    thread_arg_t a1 = {&list, 0, 50};
    thread_arg_t a2 = {&list, 50, 50};
    pthread_create(&t1, NULL, push_thread, &a1);
    pthread_create(&t2, NULL, push_thread, &a2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_concurrent_slist_count(&list) == 100);
    int found[100] = {0};
    while (!scl_concurrent_slist_empty(&list)) {
        int v;
        scl_concurrent_slist_pop_front(&list, &v);
        assert(v >= 0 && v < 100);
        found[v] = 1;
    }
    for (int i = 0; i < 100; i++) assert(found[i]);
    scl_concurrent_slist_destroy(&list);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_slist tests ===\n");
    test_init_destroy();
    test_push_pop_front();
    test_null();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
