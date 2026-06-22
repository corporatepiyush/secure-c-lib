#include "scl_skiplist.h"
#include "../../testlib/scl_test.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_insert_find(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;
    SCL_EXPECT_OK(tr, scl_skiplist_init(alloc, &sl, sizeof(int), cmp_int));
    int data[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    for (size_t i = 0; i < 10; i++)
        SCL_EXPECT_OK(tr, scl_skiplist_insert(alloc, &sl, &data[i]));
    SCL_EXPECT_EQ_SZ(tr, scl_skiplist_count(&sl), 10);
    for (int i = 0; i < 10; i++)
        SCL_EXPECT_TRUE(tr, scl_skiplist_contains(&sl, &i));
    scl_skiplist_destroy(alloc, &sl);
}

static void test_remove(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_skiplist_t sl;
    SCL_EXPECT_OK(tr, scl_skiplist_init(alloc, &sl, sizeof(int), cmp_int));
    for (int i = 0; i < 10; i++)
        SCL_EXPECT_OK(tr, scl_skiplist_insert(alloc, &sl, &i));
    int five = 5;
    SCL_EXPECT_OK(tr, scl_skiplist_remove(alloc, &sl, &five));
    SCL_EXPECT_FALSE(tr, scl_skiplist_contains(&sl, &five));
    scl_skiplist_destroy(alloc, &sl);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_skiplist tests");
    test_insert_find(&tr);
    test_remove(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
