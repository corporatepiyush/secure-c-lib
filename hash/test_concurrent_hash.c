#include "concurrent_hash.h"
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
    scl_atomic_hash_t ht;
    assert(scl_atomic_hash_init(scl_allocator_default(), &ht, sizeof(int), sizeof(int), 16, NULL) == SCL_OK);
    assert(scl_atomic_hash_count(&ht) == 0);
    scl_atomic_hash_destroy(scl_allocator_default(), &ht);
    PASS();
}

static void test_insert_get_remove(void)
{
    TEST("insert, get, remove");
    scl_atomic_hash_t ht;
    scl_atomic_hash_init(scl_allocator_default(), &ht, sizeof(int), sizeof(int), 16, NULL);
    int k, v;
    for (int i = 0; i < 100; i++) {
        k = i; v = i * 10;
        assert(scl_atomic_hash_insert(scl_allocator_default(), &ht, &k, &v) == SCL_OK);
    }
    assert(scl_atomic_hash_count(&ht) == 100);
    for (int i = 0; i < 100; i++) {
        int out;
        k = i;
        assert(scl_atomic_hash_get(&ht, &k, &out) == SCL_OK);
        assert(out == i * 10);
        assert(scl_atomic_hash_contains(&ht, &k));
    }
    for (int i = 0; i < 100; i += 2) {
        k = i;
        assert(scl_atomic_hash_remove(scl_allocator_default(), &ht, &k) == SCL_OK);
    }
    assert(scl_atomic_hash_count(&ht) == 50);
    for (int i = 1; i < 100; i += 2) {
        int out;
        k = i;
        assert(scl_atomic_hash_get(&ht, &k, &out) == SCL_OK);
    }
    scl_atomic_hash_destroy(scl_allocator_default(), &ht);
    PASS();
}

static void test_not_found(void)
{
    TEST("not found");
    scl_atomic_hash_t ht;
    scl_atomic_hash_init(scl_allocator_default(), &ht, sizeof(int), sizeof(int), 4, NULL);
    int k = 42, v;
    assert(scl_atomic_hash_get(&ht, &k, &v) == SCL_ERR_NOT_FOUND);
    assert(!scl_atomic_hash_contains(&ht, &k));
    assert(scl_atomic_hash_remove(scl_allocator_default(), &ht, &k) == SCL_ERR_NOT_FOUND);
    scl_atomic_hash_destroy(scl_allocator_default(), &ht);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_hash_init(scl_allocator_default(), NULL, sizeof(int), sizeof(int), 4, NULL) == SCL_ERR_NULL_PTR);
    scl_atomic_hash_destroy(scl_allocator_default(), NULL);
    PASS();
}

typedef struct {
    scl_atomic_hash_t *ht;
    int start;
    int count;
} thread_arg_t;

static void *insert_thread(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    for (int i = 0; i < ta->count; i++) {
        int k = ta->start + i;
        int v = k * 2;
        scl_atomic_hash_insert(scl_allocator_default(), ta->ht, &k, &v);
    }
    return NULL;
}

static void test_concurrent_insert(void)
{
    TEST("concurrent insert 2 threads x 50");
    scl_atomic_hash_t ht;
    scl_atomic_hash_init(scl_allocator_default(), &ht, sizeof(int), sizeof(int), 16, NULL);
    pthread_t t1, t2;
    thread_arg_t a1 = {&ht, 0, 50};
    thread_arg_t a2 = {&ht, 50, 50};
    pthread_create(&t1, NULL, insert_thread, &a1);
    pthread_create(&t2, NULL, insert_thread, &a2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_hash_count(&ht) == 100);
    for (int i = 0; i < 100; i++) {
        int out, k = i;
        assert(scl_atomic_hash_get(&ht, &k, &out) == SCL_OK);
        assert(out == i * 2);
    }
    scl_atomic_hash_destroy(scl_allocator_default(), &ht);
    PASS();
}

int main(void)
{
    printf("=== scl_hash tests ===\n");
    test_init_destroy();
    test_insert_get_remove();
    test_not_found();
    test_null();
    test_concurrent_insert();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
