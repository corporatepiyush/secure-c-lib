#include "concurrent_skiplist.h"
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

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_atomic_skiplist_t sl;
    assert(scl_atomic_skiplist_init(scl_allocator_default(), &sl, sizeof(int), cmp_int) == SCL_OK);
    assert(scl_atomic_skiplist_count(&sl) == 0);
    scl_atomic_skiplist_destroy(scl_allocator_default(), &sl);
    PASS();
}

static void test_insert_contains_find_remove(void)
{
    TEST("insert, contains, find, remove");
    scl_atomic_skiplist_t sl;
    scl_atomic_skiplist_init(scl_allocator_default(), &sl, sizeof(int), cmp_int);
    int vals[] = {50, 30, 70, 20, 40, 60, 80};
    for (int i = 0; i < 7; i++) assert(scl_atomic_skiplist_insert(scl_allocator_default(), &sl, &vals[i]) == SCL_OK);
    assert(scl_atomic_skiplist_count(&sl) == 7);
    for (int i = 0; i < 7; i++) assert(scl_atomic_skiplist_contains(&sl, &vals[i]));
    int v;
    for (int i = 0; i < 7; i++) { assert(scl_atomic_skiplist_find(&sl, &vals[i], &v) == SCL_OK); assert(v == vals[i]); }
    assert(!scl_atomic_skiplist_contains(&sl, &(int){99}));
    assert(scl_atomic_skiplist_remove(scl_allocator_default(), &sl, &(int){99}) == SCL_ERR_NOT_FOUND);
    assert(scl_atomic_skiplist_remove(scl_allocator_default(), &sl, &vals[0]) == SCL_OK);
    assert(scl_atomic_skiplist_count(&sl) == 6);
    assert(!scl_atomic_skiplist_contains(&sl, &vals[0]));
    scl_atomic_skiplist_destroy(scl_allocator_default(), &sl);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_skiplist_init(scl_allocator_default(), NULL, sizeof(int), cmp_int) == SCL_ERR_NULL_PTR);
    scl_atomic_skiplist_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *insert_thread(void *arg)
{
    scl_atomic_skiplist_t *sl = (scl_atomic_skiplist_t *)arg;
    for (int i = 0; i < 50; i++) scl_atomic_skiplist_insert(scl_allocator_default(), sl, &i);
    return NULL;
}

static void test_concurrent_insert(void)
{
    TEST("concurrent insert 2 threads");
    scl_atomic_skiplist_t sl;
    scl_atomic_skiplist_init(scl_allocator_default(), &sl, sizeof(int), cmp_int);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, insert_thread, &sl);
    pthread_create(&t2, NULL, insert_thread, &sl);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_skiplist_count(&sl) == 50);
    for (int i = 0; i < 50; i++) assert(scl_atomic_skiplist_contains(&sl, &i));
    scl_atomic_skiplist_destroy(scl_allocator_default(), &sl);
    PASS();
}

int main(void)
{
    printf("=== scl_skiplist tests ===\n");
    test_init_destroy();
    test_insert_contains_find_remove();
    test_null();
    test_concurrent_insert();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
