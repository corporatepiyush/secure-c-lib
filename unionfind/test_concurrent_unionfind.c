#include "concurrent_unionfind.h"
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
    scl_atomic_unionfind_t uf;
    assert(scl_atomic_unionfind_init(scl_allocator_default(), &uf, 10) == SCL_OK);
    assert(scl_atomic_unionfind_sets(&uf) == 10);
    assert(scl_atomic_unionfind_count(&uf) == 10);
    scl_atomic_unionfind_destroy(scl_allocator_default(), &uf);
    PASS();
}

static void test_find_union_connected(void)
{
    TEST("find, union, connected");
    scl_atomic_unionfind_t uf;
    scl_atomic_unionfind_init(scl_allocator_default(), &uf, 10);
    assert(scl_atomic_unionfind_union(&uf, 0, 1) == SCL_OK);
    assert(scl_atomic_unionfind_union(&uf, 2, 3) == SCL_OK);
    assert(scl_atomic_unionfind_union(&uf, 0, 2) == SCL_OK);
    assert(scl_atomic_unionfind_connected(&uf, 0, 3));
    assert(scl_atomic_unionfind_connected(&uf, 1, 2));
    assert(!scl_atomic_unionfind_connected(&uf, 0, 4));
    assert(scl_atomic_unionfind_sets(&uf) == 7);
    for (size_t i = 0; i < 10; i++) {
        size_t r = scl_atomic_unionfind_find(&uf, i);
        assert(r < 10);
    }
    scl_atomic_unionfind_destroy(scl_allocator_default(), &uf);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_unionfind_init(scl_allocator_default(), NULL, 10) == SCL_ERR_NULL_PTR);
    scl_atomic_unionfind_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *union_thread(void *arg)
{
    scl_atomic_unionfind_t *uf = (scl_atomic_unionfind_t *)arg;
    for (size_t i = 0; i < 50; i += 2) scl_atomic_unionfind_union(uf, i, i + 1);
    return NULL;
}

static void test_concurrent_union(void)
{
    TEST("concurrent union");
    scl_atomic_unionfind_t uf;
    scl_atomic_unionfind_init(scl_allocator_default(), &uf, 100);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, union_thread, &uf);
    pthread_create(&t2, NULL, union_thread, &uf);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_unionfind_sets(&uf) <= 100);
    scl_atomic_unionfind_destroy(scl_allocator_default(), &uf);
    PASS();
}

int main(void)
{
    printf("=== scl_unionfind tests ===\n");
    test_init_destroy();
    test_find_union_connected();
    test_null();
    test_concurrent_union();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
