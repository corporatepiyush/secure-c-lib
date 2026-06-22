#include "concurrent_trie.h"
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
    scl_atomic_trie_t trie;
    assert(scl_atomic_trie_init(scl_allocator_default(), &trie, sizeof(int)) == SCL_OK);
    assert(scl_atomic_trie_count(&trie) == 0);
    scl_atomic_trie_destroy(scl_allocator_default(), &trie);
    PASS();
}

static void test_insert_get_contains_remove(void)
{
    TEST("insert, get, contains, remove");
    scl_atomic_trie_t trie;
    scl_atomic_trie_init(scl_allocator_default(), &trie, sizeof(int));
    const unsigned char *key1 = (const unsigned char *)"hello";
    const unsigned char *key2 = (const unsigned char *)"world";
    const unsigned char *key3 = (const unsigned char *)"help";
    int v1 = 10, v2 = 20, v3 = 30;
    assert(scl_atomic_trie_insert(scl_allocator_default(), &trie, key1, 5, &v1) == SCL_OK);
    assert(scl_atomic_trie_insert(scl_allocator_default(), &trie, key2, 5, &v2) == SCL_OK);
    assert(scl_atomic_trie_insert(scl_allocator_default(), &trie, key3, 4, &v3) == SCL_OK);
    assert(scl_atomic_trie_count(&trie) == 3);
    int out;
    assert(scl_atomic_trie_get(&trie, key1, 5, &out) == SCL_OK && out == 10);
    assert(scl_atomic_trie_get(&trie, key2, 5, &out) == SCL_OK && out == 20);
    assert(scl_atomic_trie_get(&trie, key3, 4, &out) == SCL_OK && out == 30);
    assert(scl_atomic_trie_contains(&trie, key1, 5));
    assert(!scl_atomic_trie_contains(&trie, (const unsigned char *)"none", 4));
    assert(scl_atomic_trie_get(&trie, (const unsigned char *)"none", 4, &out) == SCL_ERR_NOT_FOUND);
    assert(scl_atomic_trie_remove(scl_allocator_default(), &trie, key1, 5) == SCL_OK);
    assert(!scl_atomic_trie_contains(&trie, key1, 5));
    assert(scl_atomic_trie_count(&trie) == 2);
    scl_atomic_trie_destroy(scl_allocator_default(), &trie);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_trie_init(scl_allocator_default(), NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    scl_atomic_trie_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *insert_thread(void *arg)
{
    scl_atomic_trie_t *trie = (scl_atomic_trie_t *)arg;
    char key[32];
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        int v = i;
        scl_atomic_trie_insert(scl_allocator_default(), trie, (const unsigned char *)key, strlen(key), &v);
    }
    return NULL;
}

static void test_concurrent_insert(void)
{
    TEST("concurrent insert 2 threads");
    scl_atomic_trie_t trie;
    scl_atomic_trie_init(scl_allocator_default(), &trie, sizeof(int));
    pthread_t t1, t2;
    pthread_create(&t1, NULL, insert_thread, &trie);
    pthread_create(&t2, NULL, insert_thread, &trie);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_trie_count(&trie) == 50);
    scl_atomic_trie_destroy(scl_allocator_default(), &trie);
    PASS();
}

int main(void)
{
    printf("=== scl_trie tests ===\n");
    test_init_destroy();
    test_insert_get_contains_remove();
    test_null();
    test_concurrent_insert();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
