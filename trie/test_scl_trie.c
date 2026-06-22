#include "scl_trie.h"
#include "../../testlib/scl_test.h"

static void test_insert_search(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t t;
    SCL_EXPECT_OK(tr, scl_trie_init(alloc, &t, sizeof(int)));
    int v = 1;
    SCL_EXPECT_OK(tr, scl_trie_insert(alloc, &t, (const unsigned char *)"hello", 5, &v));
    SCL_EXPECT_OK(tr, scl_trie_insert(alloc, &t, (const unsigned char *)"world", 5, &v));
    SCL_EXPECT_OK(tr, scl_trie_insert(alloc, &t, (const unsigned char *)"hi", 2, &v));
    SCL_EXPECT_TRUE(tr, scl_trie_contains(&t, (const unsigned char *)"hello", 5));
    int out;
    SCL_EXPECT_OK(tr, scl_trie_get(&t, (const unsigned char *)"hello", 5, &out));
    SCL_EXPECT_FALSE(tr, scl_trie_contains(&t, (const unsigned char *)"notfound", 8));
    SCL_EXPECT_EQ_SZ(tr, scl_trie_count(&t), 3);
    scl_trie_destroy(alloc, &t);
}

static void test_remove(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_trie_t t;
    SCL_EXPECT_OK(tr, scl_trie_init(alloc, &t, sizeof(int)));
    int v = 1;
    SCL_EXPECT_OK(tr, scl_trie_insert(alloc, &t, (const unsigned char *)"test", 4, &v));
    SCL_EXPECT_TRUE(tr, scl_trie_contains(&t, (const unsigned char *)"test", 4));
    SCL_EXPECT_OK(tr, scl_trie_remove(alloc, &t, (const unsigned char *)"test", 4));
    SCL_EXPECT_FALSE(tr, scl_trie_contains(&t, (const unsigned char *)"test", 4));
    scl_trie_destroy(alloc, &t);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_trie tests");
    test_insert_search(&tr);
    test_remove(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
