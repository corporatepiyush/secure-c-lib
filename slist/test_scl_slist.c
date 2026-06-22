#include "scl_slist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_slist_t list;
    assert(scl_slist_init(&list, sizeof(int)) == SCL_OK);
    assert(scl_slist_empty(&list));
    scl_slist_destroy(&list);
    PASS();
}

static void test_push_front_pop_front(void)
{
    TEST("push front/pop front");
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++)
        assert(scl_slist_push_front(&list, &i) == SCL_OK);
    assert(scl_slist_count(&list) == 10);
    for (int i = 9; i >= 0; i--) {
        int val; scl_slist_pop_front(&list, &val);
        assert(val == i);
    }
    assert(scl_slist_empty(&list));
    scl_slist_destroy(&list);
    PASS();
}

static void test_push_back_pop_front(void)
{
    TEST("push back/pop front (FIFO)");
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++)
        assert(scl_slist_push_back(&list, &i) == SCL_OK);
    for (int i = 0; i < 10; i++) {
        int val; scl_slist_pop_front(&list, &val);
        assert(val == i);
    }
    scl_slist_destroy(&list);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_slist_init(NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    assert(scl_slist_init(NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    scl_slist_destroy(NULL);
    PASS();

    TEST("zero element size rejection");
    scl_slist_t list;
    assert(scl_slist_init(&list, 0) == SCL_ERR_INVALID_ARG);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++) scl_slist_push_back(&list, &i);
    int key = 5, out;
    assert(scl_slist_search(&list, &key, cmp_int, &out) == SCL_OK && out == 5);
    key = 999;
    assert(scl_slist_search(&list, &key, cmp_int, &out) == SCL_ERR_NOT_FOUND);
    scl_slist_destroy(&list);
    PASS();
}

int main(void)
{
    printf("=== scl_slist tests ===\n");
    test_init_destroy();
    test_push_front_pop_front();
    test_push_back_pop_front();
    test_null();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
