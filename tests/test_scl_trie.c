#include "scl_test.h"
#include "scl_trie.h"

static void test_trie_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Trie: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t trie;

    scl_error_t err = scl_trie_init(alloc, &trie, sizeof(int));
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_trie_count(&trie), 0);

    scl_trie_destroy(alloc, &trie);
}

static void test_trie_insert_get(scl_test_runner_t *tr) {
    scl_test_group("Trie: insert and get");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t trie;
    scl_trie_init(alloc, &trie, sizeof(int));

    const unsigned char *key = (const unsigned char *)"hello";
    int val = 42;
    scl_error_t err = scl_trie_insert(alloc, &trie, key, 5, &val);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_trie_count(&trie), 1);

    int out;
    err = scl_trie_get(&trie, key, 5, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_trie_destroy(alloc, &trie);
}

static void test_trie_contains(scl_test_runner_t *tr) {
    scl_test_group("Trie: contains check");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t trie;
    scl_trie_init(alloc, &trie, sizeof(int));

    const unsigned char *key = (const unsigned char *)"test";
    int val = 10;
    scl_trie_insert(alloc, &trie, key, 4, &val);

    SCL_EXPECT_TRUE(tr, scl_trie_contains(&trie, key, 4));

    const unsigned char *other = (const unsigned char *)"other";
    SCL_EXPECT_FALSE(tr, scl_trie_contains(&trie, other, 5));

    scl_trie_destroy(alloc, &trie);
}

static void test_trie_remove(scl_test_runner_t *tr) {
    scl_test_group("Trie: remove");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t trie;
    scl_trie_init(alloc, &trie, sizeof(int));

    const unsigned char *key = (const unsigned char *)"remove";
    int val = 99;
    scl_trie_insert(alloc, &trie, key, 6, &val);
    SCL_EXPECT_EQ_SZ(tr, scl_trie_count(&trie), 1);

    scl_error_t err = scl_trie_remove(alloc, &trie, key, 6);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_trie_count(&trie), 0);
    SCL_EXPECT_FALSE(tr, scl_trie_contains(&trie, key, 6));

    scl_trie_destroy(alloc, &trie);
}

static void test_trie_multiple_inserts(scl_test_runner_t *tr) {
    scl_test_group("Trie: multiple inserts");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t trie;
    scl_trie_init(alloc, &trie, sizeof(int));

    const unsigned char *keys[] = {
        (const unsigned char *)"apple",
        (const unsigned char *)"app",
        (const unsigned char *)"application"
    };

    for (int i = 0; i < 3; i++) {
        int val = i + 1;
        scl_trie_insert(alloc, &trie, keys[i], i < 2 ? 3 + i : 11, &val);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_trie_count(&trie), 3);

    scl_trie_destroy(alloc, &trie);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_trie_init_destroy(&tr);
    test_trie_insert_get(&tr);
    test_trie_contains(&tr);
    test_trie_remove(&tr);
    test_trie_multiple_inserts(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
