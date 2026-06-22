#include "scl_slist.h"
#include "../testlib/scl_test.h"
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_init_destroy(scl_test_runner_t *tr)
{
    scl_test_group("init and destroy");
    scl_allocator_t *a = scl_allocator_default();
    scl_slist_t list;
    SCL_EXPECT_OK(tr, scl_slist_init(&list, sizeof(int)));
    SCL_EXPECT_TRUE(tr, scl_slist_empty(&list));
    scl_slist_destroy(a, &list);
}

static void test_push_front_pop_front(scl_test_runner_t *tr)
{
    scl_test_group("push front/pop front");
    scl_allocator_t *a = scl_allocator_default();
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++)
        SCL_EXPECT_OK(tr, scl_slist_push_front(a, &list, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_slist_count(&list), 10);
    for (int i = 9; i >= 0; i--) {
        int val; scl_slist_pop_front(a, &list, &val);
        SCL_EXPECT_EQ_I(tr, val, i);
    }
    SCL_EXPECT_TRUE(tr, scl_slist_empty(&list));
    scl_slist_destroy(a, &list);
}

static void test_push_back_pop_front(scl_test_runner_t *tr)
{
    scl_test_group("push back/pop front (FIFO)");
    scl_allocator_t *a = scl_allocator_default();
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++)
        SCL_EXPECT_OK(tr, scl_slist_push_back(a, &list, &i));
    for (int i = 0; i < 10; i++) {
        int val; scl_slist_pop_front(a, &list, &val);
        SCL_EXPECT_EQ_I(tr, val, i);
    }
    scl_slist_destroy(a, &list);
}

static void test_null(scl_test_runner_t *tr)
{
    scl_test_group("null checks");
    SCL_EXPECT_ERROR(tr, scl_slist_init(NULL, sizeof(int)), SCL_ERR_NULL_PTR);
    scl_allocator_t *a = scl_allocator_default();
    scl_slist_destroy(a, NULL);

    scl_test_group("zero element size rejection");
    scl_slist_t list;
    SCL_EXPECT_ERROR(tr, scl_slist_init(&list, 0), SCL_ERR_INVALID_ARG);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_slist_t list;
    scl_slist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++) scl_slist_push_back(a, &list, &i);
    int key = 5, out;
    SCL_EXPECT_OK(tr, scl_slist_search(&list, &key, cmp_int, &out));
    SCL_EXPECT_EQ_I(tr, out, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_slist_search(&list, &key, cmp_int, &out), SCL_ERR_NOT_FOUND);
    scl_slist_destroy(a, &list);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_slist tests ===");
    test_init_destroy(&tr);
    test_push_front_pop_front(&tr);
    test_push_back_pop_front(&tr);
    test_null(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
