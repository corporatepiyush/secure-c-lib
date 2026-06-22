#include "scl_rbtree.h"
#include "../../testlib/scl_test.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void visit_fill(void *data, void *ctx)
{
    int *arr = (int *)ctx;
    arr[*(int *)data] = *(int *)data;
}

static void test_insert_contains(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t t;
    SCL_EXPECT_OK(tr, scl_rbtree_init(alloc, &t, sizeof(int), cmp_int));
    for (int i = 0; i < 100; i++)
        SCL_EXPECT_OK(tr, scl_rbtree_insert(alloc, &t, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&t), 100);
    for (int i = 0; i < 100; i++)
        SCL_EXPECT_TRUE(tr, scl_rbtree_contains(&t, &i));
    scl_rbtree_destroy(alloc, &t);
}

static void test_remove(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t t;
    SCL_EXPECT_OK(tr, scl_rbtree_init(alloc, &t, sizeof(int), cmp_int));
    for (int i = 0; i < 50; i++)
        SCL_EXPECT_OK(tr, scl_rbtree_insert(alloc, &t, &i));
    for (int i = 0; i < 50; i += 2)
        SCL_EXPECT_OK(tr, scl_rbtree_remove(alloc, &t, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&t), 25);
    scl_rbtree_destroy(alloc, &t);
}

static void test_inorder(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_rbtree_t t;
    SCL_EXPECT_OK(tr, scl_rbtree_init(alloc, &t, sizeof(int), cmp_int));
    int data[] = {5, 3, 7, 2, 4, 6, 8};
    for (size_t i = 0; i < 7; i++)
        SCL_EXPECT_OK(tr, scl_rbtree_insert(alloc, &t, &data[i]));
    SCL_EXPECT_EQ_SZ(tr, scl_rbtree_count(&t), 7);
    int sorted[7] = {0};
    SCL_EXPECT_OK(tr, scl_rbtree_inorder(&t, visit_fill, sorted));
    scl_rbtree_destroy(alloc, &t);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_rbtree tests");
    test_insert_contains(&tr);
    test_remove(&tr);
    test_inorder(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
