#include "scl_hash.h"
#include "../../testlib/scl_test.h"

SCL_UNUSED static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_insert_get(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_hash_t ht;
    SCL_EXPECT_OK(tr, scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16, scl_hash_djb2, NULL));
    for (int i = 0; i < 100; i++)
        SCL_EXPECT_OK(tr, scl_hash_insert(alloc, &ht, &i, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_hash_count(&ht), 100);
    for (int i = 0; i < 100; i++) {
        int v;
        SCL_EXPECT_OK(tr, scl_hash_get(&ht, &i, &v));
        SCL_EXPECT_EQ_I(tr, v, i);
    }
    scl_hash_destroy(alloc, &ht);
}

static void test_remove_contains(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_hash_t ht;
    SCL_EXPECT_OK(tr, scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16, scl_hash_djb2, NULL));
    int k = 42, v = 99;
    SCL_EXPECT_OK(tr, scl_hash_insert(alloc, &ht, &k, &v));
    SCL_EXPECT_TRUE(tr, scl_hash_contains(&ht, &k));
    SCL_EXPECT_OK(tr, scl_hash_remove(alloc, &ht, &k));
    SCL_EXPECT_FALSE(tr, scl_hash_contains(&ht, &k));
    scl_hash_destroy(alloc, &ht);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_hash tests");
    test_insert_get(&tr);
    test_remove_contains(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
