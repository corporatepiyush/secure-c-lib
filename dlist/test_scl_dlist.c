#include "scl_dlist.h"
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
    scl_dlist_t list;
    assert(scl_dlist_init(&list, sizeof(int)) == SCL_OK);
    scl_dlist_destroy(&list);
    PASS();
}

static void test_push_pop(void)
{
    TEST("push/pop front and back");
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));
    for (int i = 0; i < 5; i++) {
        assert(scl_dlist_push_front(&list, &i) == SCL_OK);
        assert(scl_dlist_push_back(&list, &i) == SCL_OK);
    }
    assert(scl_dlist_count(&list) == 10);
    for (int i = 4; i >= 0; i--) {
        int v; scl_dlist_pop_front(&list, &v); assert(v == i);
        scl_dlist_pop_back(&list, &v); assert(v == i);
    }
    scl_dlist_destroy(&list);
    PASS();
}

static void test_insert_remove_at(void)
{
    TEST("insert at and remove at");
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));
    for (int i = 0; i < 5; i++) scl_dlist_push_back(&list, &i);
    int v = 99; assert(scl_dlist_insert_at(&list, 2, &v) == SCL_OK);
    scl_dlist_remove_at(&list, 2, &v); assert(v == 99);
    scl_dlist_destroy(&list);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++) scl_dlist_push_back(&list, &i);
    int key = 5, out;
    assert(scl_dlist_search(&list, &key, cmp_int, &out) == SCL_OK && out == 5);
    key = 999;
    assert(scl_dlist_search(&list, &key, cmp_int, &out) == SCL_ERR_NOT_FOUND);
    scl_dlist_destroy(&list);
    PASS();
}

int main(void)
{
    printf("=== scl_dlist tests ===\n");
    test_init_destroy();
    test_push_pop();
    test_insert_remove_at();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
