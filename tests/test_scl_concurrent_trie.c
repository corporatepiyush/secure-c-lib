#include "scl_test.h"
#include "scl_concurrent_trie.h"
#include "scl_pthread.h"
#include "scl_atomic.h"
#include <string.h>

#define NTHREADS 4
#define OPS_PER_THREAD 200

static void test_ctrie_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CTrie: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_trie_t t;
    scl_error_t err = scl_ctrie_init(alloc, &t, sizeof(int));
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_ctrie_count(&t), 0);
    scl_ctrie_destroy(alloc, &t);
}

static void test_ctrie_insert_get(scl_test_runner_t *tr) {
    scl_test_group("CTrie: insert and get");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_trie_t t;
    scl_ctrie_init(alloc, &t, sizeof(int));

    unsigned char key[] = "hello";
    int v = 42;
    SCL_EXPECT_OK(tr, scl_ctrie_insert(alloc, &t, key, 5, &v));
    SCL_EXPECT_TRUE(tr, scl_ctrie_contains(&t, key, 5));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_ctrie_get(&t, key, 5, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_ctrie_destroy(alloc, &t);
}

static void test_ctrie_remove(scl_test_runner_t *tr) {
    scl_test_group("CTrie: remove");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_trie_t t;
    scl_ctrie_init(alloc, &t, sizeof(int));

    unsigned char key[] = "test";
    int v = 7;
    scl_ctrie_insert(alloc, &t, key, 4, &v);
    SCL_EXPECT_TRUE(tr, scl_ctrie_contains(&t, key, 4));
    SCL_EXPECT_OK(tr, scl_ctrie_remove(alloc, &t, key, 4));
    SCL_EXPECT_FALSE(tr, scl_ctrie_contains(&t, key, 4));
    SCL_EXPECT_EQ_SZ(tr, scl_ctrie_count(&t), 0);

    scl_ctrie_destroy(alloc, &t);
}

static void test_ctrie_missing(scl_test_runner_t *tr) {
    scl_test_group("CTrie: get missing returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_trie_t t;
    scl_ctrie_init(alloc, &t, sizeof(int));
    unsigned char key[] = "missing";
    int out;
    SCL_EXPECT_TRUE(tr, scl_ctrie_get(&t, key, 7, &out) != SCL_OK);
    scl_ctrie_destroy(alloc, &t);
}

static void test_ctrie_multiple(scl_test_runner_t *tr) {
    scl_test_group("CTrie: multiple entries");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_trie_t t;
    scl_ctrie_init(alloc, &t, sizeof(int));

    for (int i = 0; i < 20; i++) {
        unsigned char key[16];
        int n = snprintf((char *)key, sizeof(key), "key%d", i);
        scl_ctrie_insert(alloc, &t, key, (size_t)n, &i);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_ctrie_count(&t), 20);

    for (int i = 0; i < 20; i++) {
        unsigned char key[16];
        int n = snprintf((char *)key, sizeof(key), "key%d", i);
        int out = -1;
        SCL_EXPECT_OK(tr, scl_ctrie_get(&t, key, (size_t)n, &out));
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_ctrie_destroy(alloc, &t);
}

typedef struct { scl_concurrent_trie_t *t; int base; } ctrie_arg_t;

static void *ctrie_insert_thread(void *arg) {
    ctrie_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        unsigned char key[16];
        int k = a->base + i;
        int n = snprintf((char *)key, sizeof(key), "k%d", k);
        scl_ctrie_insert(alloc, a->t, key, (size_t)n, &k);
    }
    return NULL;
}

static void test_ctrie_concurrent_insert(scl_test_runner_t *tr) {
    scl_test_group("CTrie: concurrent inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_trie_t t;
    scl_ctrie_init(alloc, &t, sizeof(int));

    pthread_t threads[NTHREADS];
    ctrie_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (ctrie_arg_t){.t = &t, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, ctrie_insert_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    SCL_EXPECT_EQ_SZ(tr, scl_ctrie_count(&t), (size_t)(NTHREADS * OPS_PER_THREAD));
    scl_ctrie_destroy(alloc, &t);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_ctrie_init_destroy(&tr);
    test_ctrie_insert_get(&tr);
    test_ctrie_remove(&tr);
    test_ctrie_missing(&tr);
    test_ctrie_multiple(&tr);
    test_ctrie_concurrent_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
