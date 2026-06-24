#include "scl_test.h"
#include "scl_concurrent_hash.h"
#include "scl_pthread.h"
#include "scl_atomic.h"
#include <string.h>

#define NTHREADS 4
#define OPS_PER_THREAD 500

static size_t int_hash(const void *key, size_t len) {
    size_t k;
    if (len >= sizeof(size_t)) {
        k = *(const size_t *)key;
    } else if (len == 4) {
        k = (size_t)*(const uint32_t *)key;
    } else if (len == 2) {
        k = (size_t)*(const uint16_t *)key;
    } else if (len == 1) {
        k = (size_t)*(const uint8_t *)key;
    } else {
        k = 0;
    }
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    return k;
}

static void test_chash_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CHash: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_hash_t ht;
    scl_error_t err = scl_chash_init(alloc, &ht, sizeof(int), sizeof(int), 16, int_hash);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_chash_count(&ht), 0);
    scl_chash_destroy(alloc, &ht);
}

static void test_chash_insert_get(scl_test_runner_t *tr) {
    scl_test_group("CHash: insert and get");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_hash_t ht;
    scl_chash_init(alloc, &ht, sizeof(int), sizeof(int), 16, int_hash);

    int k = 7, v = 42;
    SCL_EXPECT_OK(tr, scl_chash_insert(alloc, &ht, &k, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_chash_count(&ht), 1);
    SCL_EXPECT_TRUE(tr, scl_chash_contains(&ht, &k));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_chash_get(&ht, &k, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_chash_destroy(alloc, &ht);
}

static void test_chash_remove(scl_test_runner_t *tr) {
    scl_test_group("CHash: remove");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_hash_t ht;
    scl_chash_init(alloc, &ht, sizeof(int), sizeof(int), 16, int_hash);

    int k = 3, v = 30;
    scl_chash_insert(alloc, &ht, &k, &v);
    SCL_EXPECT_TRUE(tr, scl_chash_contains(&ht, &k));

    SCL_EXPECT_OK(tr, scl_chash_remove(alloc, &ht, &k));
    SCL_EXPECT_FALSE(tr, scl_chash_contains(&ht, &k));
    SCL_EXPECT_EQ_SZ(tr, scl_chash_count(&ht), 0);

    scl_chash_destroy(alloc, &ht);
}

static void test_chash_missing_key(scl_test_runner_t *tr) {
    scl_test_group("CHash: get missing key returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_hash_t ht;
    scl_chash_init(alloc, &ht, sizeof(int), sizeof(int), 16, int_hash);

    int k = 999, out;
    scl_error_t err = scl_chash_get(&ht, &k, &out);
    SCL_EXPECT_TRUE(tr, err != SCL_OK);

    scl_chash_destroy(alloc, &ht);
}

static void test_chash_multiple_entries(scl_test_runner_t *tr) {
    scl_test_group("CHash: multiple entries");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_hash_t ht;
    scl_chash_init(alloc, &ht, sizeof(int), sizeof(int), 32, int_hash);

    for (int i = 0; i < 50; i++)
        scl_chash_insert(alloc, &ht, &i, &i);

    SCL_EXPECT_EQ_SZ(tr, scl_chash_count(&ht), 50);
    for (int i = 0; i < 50; i++) {
        int out = -1;
        SCL_EXPECT_OK(tr, scl_chash_get(&ht, &i, &out));
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_chash_destroy(alloc, &ht);
}

typedef struct { scl_concurrent_hash_t *ht; int base; } chash_targ_t;

static void *insert_thread(void *arg) {
    chash_targ_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int k = a->base + i;
        int v = k * 2;
        scl_chash_insert(alloc, a->ht, &k, &v);
    }
    return NULL;
}

static void test_chash_concurrent_insert(scl_test_runner_t *tr) {
    scl_test_group("CHash: concurrent inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_hash_t ht;
    scl_chash_init(alloc, &ht, sizeof(int), sizeof(int), 64, int_hash);

    pthread_t threads[NTHREADS];
    chash_targ_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (chash_targ_t){.ht = &ht, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, insert_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    SCL_EXPECT_EQ_SZ(tr, scl_chash_count(&ht), (size_t)(NTHREADS * OPS_PER_THREAD));
    scl_chash_destroy(alloc, &ht);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_chash_init_destroy(&tr);
    test_chash_insert_get(&tr);
    test_chash_remove(&tr);
    test_chash_missing_key(&tr);
    test_chash_multiple_entries(&tr);
    test_chash_concurrent_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
