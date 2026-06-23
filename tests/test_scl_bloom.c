#include "scl_test.h"
#include "scl_bloom.h"

static void test_bloom_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Bloom: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bloom_t bf;

    scl_error_t err = scl_bloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_bloom_count(&bf), 0);

    scl_bloom_destroy(alloc, &bf);
}

static void test_bloom_insert_contains(scl_test_runner_t *tr) {
    scl_test_group("Bloom: insert and contains");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bloom_t bf;
    scl_bloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);

    const char *data1 = "hello";
    const char *data2 = "world";
    const char *data3 = "missing";

    scl_error_t err = scl_bloom_insert(&bf, data1, 5);
    SCL_EXPECT_OK(tr, err);
    err = scl_bloom_insert(&bf, data2, 5);
    SCL_EXPECT_OK(tr, err);

    SCL_EXPECT_TRUE(tr, scl_bloom_maybe_contains(&bf, data1, 5));
    SCL_EXPECT_TRUE(tr, scl_bloom_maybe_contains(&bf, data2, 5));
    SCL_EXPECT_FALSE(tr, scl_bloom_maybe_contains(&bf, data3, 7));

    scl_bloom_destroy(alloc, &bf);
}

static void test_bloom_clear(scl_test_runner_t *tr) {
    scl_test_group("Bloom: clear");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bloom_t bf;
    scl_bloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);

    const char *data = "test";
    scl_bloom_insert(&bf, data, 4);
    SCL_EXPECT_TRUE(tr, scl_bloom_maybe_contains(&bf, data, 4));

    scl_bloom_clear(&bf);
    SCL_EXPECT_EQ_SZ(tr, scl_bloom_count(&bf), 0);

    scl_bloom_destroy(alloc, &bf);
}

static void test_bloom_multiple_inserts(scl_test_runner_t *tr) {
    scl_test_group("Bloom: multiple inserts");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_bloom_t bf;
    scl_bloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);

    const char *items[] = {"item1", "item2", "item3", "item4", "item5"};
    for (int i = 0; i < 5; i++) {
        scl_bloom_insert(&bf, items[i], 5);
    }

    for (int i = 0; i < 5; i++) {
        SCL_EXPECT_TRUE(tr, scl_bloom_maybe_contains(&bf, items[i], 5));
    }

    scl_bloom_destroy(alloc, &bf);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_bloom_init_destroy(&tr);
    test_bloom_insert_contains(&tr);
    test_bloom_clear(&tr);
    test_bloom_multiple_inserts(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
