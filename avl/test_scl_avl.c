#include "scl_avl.h"
#include "../../testlib/scl_test.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_insert_balance(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t t;
    SCL_EXPECT_OK(tr, scl_avl_init(alloc, &t, sizeof(int), cmp_int));
    for (int i = 0; i < 100; i++)
        SCL_EXPECT_OK(tr, scl_avl_insert(alloc, &t, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&t), 100);
    for (int i = 0; i < 100; i++)
        SCL_EXPECT_TRUE(tr, scl_avl_contains(&t, &i));
    scl_avl_destroy(alloc, &t);
}

static void test_remove(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_avl_t t;
    SCL_EXPECT_OK(tr, scl_avl_init(alloc, &t, sizeof(int), cmp_int));
    for (int i = 0; i < 50; i++)
        SCL_EXPECT_OK(tr, scl_avl_insert(alloc, &t, &i));
    for (int i = 0; i < 50; i += 2)
        SCL_EXPECT_OK(tr, scl_avl_remove(alloc, &t, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_avl_count(&t), 25);
    scl_avl_destroy(alloc, &t);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_avl tests");
    test_insert_balance(&tr);
    test_remove(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
