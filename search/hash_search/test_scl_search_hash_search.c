#include "../../testlib/scl_test.h"
#include "scl_search_hash_search.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();

    {
        scl_search_ht_t *ht = NULL;
        scl_test_group("hash_search");
        SCL_EXPECT_OK(&tr, scl_search_ht_init(a, &ht, 16));
        SCL_EXPECT_NOT_NULL(&tr, ht);
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(a, &ht, 16);
        int v = 42;
        (void)scl_search_ht_insert(ht, "key1", &v);
        void *out;
        SCL_EXPECT_OK(&tr, scl_search_ht_search(ht, "key1", &out));
        SCL_EXPECT_EQ_I(&tr, 42, *(int*)out);
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(a, &ht, 16);
        int v = 1;
        (void)scl_search_ht_insert(ht, "k", &v);
        void *out;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_ht_search(ht, "nope", &out));
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(a, &ht, 16);
        int v1 = 1, v2 = 2;
        (void)scl_search_ht_insert(ht, "a", &v1);
        (void)scl_search_ht_insert(ht, "b", &v2);
        void *out;
        SCL_EXPECT_OK(&tr, scl_search_ht_search(ht, "b", &out));
        SCL_EXPECT_EQ_I(&tr, 2, *(int*)out);
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(a, &ht, 16);
        int v = 7;
        (void)scl_search_ht_insert(ht, "del", &v);
        SCL_EXPECT_OK(&tr, scl_search_ht_delete(ht, "del"));
        void *out;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_ht_search(ht, "del", &out));
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(a, &ht, 16);
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_ht_search(NULL, "x", (void**)(uintptr_t)1));
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(a, &ht, 16);
        int v = 10;
        (void)scl_search_ht_insert(ht, "update", &v);
        int v2 = 20;
        (void)scl_search_ht_insert(ht, "update", &v2);
        void *out;
        SCL_EXPECT_OK(&tr, scl_search_ht_search(ht, "update", &out));
        SCL_EXPECT_EQ_I(&tr, 20, *(int*)out);
        scl_search_ht_destroy(ht);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
