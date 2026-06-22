#include "../../testlib/scl_test.h"
#include "scl_search_trie_search.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();

    {
        scl_search_trie_t *t = NULL;
        scl_test_group("trie");
        SCL_EXPECT_OK(&tr, scl_search_trie_init(a, &t));
        SCL_EXPECT_NOT_NULL(&tr, t);
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "hello");
        SCL_EXPECT_TRUE(&tr, scl_search_trie_search(t, "hello"));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "hello");
        SCL_EXPECT_FALSE(&tr, scl_search_trie_search(t, "world"));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "hello");
        SCL_EXPECT_TRUE(&tr, scl_search_trie_starts_with(t, "hel"));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "hello");
        SCL_EXPECT_FALSE(&tr, scl_search_trie_starts_with(t, "xyz"));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "hello");
        (void)scl_search_trie_insert(t, "help");
        (void)scl_search_trie_insert(t, "world");
        scl_search_trie_delete(t, "hello");
        SCL_EXPECT_FALSE(&tr, scl_search_trie_search(t, "hello"));
        SCL_EXPECT_TRUE(&tr, scl_search_trie_search(t, "help"));
        SCL_EXPECT_TRUE(&tr, scl_search_trie_search(t, "world"));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "");
        SCL_EXPECT_TRUE(&tr, scl_search_trie_search(t, ""));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        SCL_EXPECT_FALSE(&tr, scl_search_trie_search(NULL, "a"));
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(a, &t);
        (void)scl_search_trie_insert(t, "a");
        SCL_EXPECT_TRUE(&tr, scl_search_trie_search(t, "a"));
        scl_search_trie_destroy(t);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
