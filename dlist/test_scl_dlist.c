#include "scl_dlist.h"
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
    scl_dlist_t list;
    SCL_EXPECT_OK(tr, scl_dlist_init(&list, sizeof(int)));
    scl_dlist_destroy(a, &list);
}

static void test_push_pop(scl_test_runner_t *tr)
{
    scl_test_group("push/pop front and back");
    scl_allocator_t *a = scl_allocator_default();
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));
    for (int i = 0; i < 5; i++) {
        SCL_EXPECT_OK(tr, scl_dlist_push_front(a, &list, &i));
        SCL_EXPECT_OK(tr, scl_dlist_push_back(a, &list, &i));
    }
    SCL_EXPECT_EQ_SZ(tr, scl_dlist_count(&list), 10);
    for (int i = 4; i >= 0; i--) {
        int v; scl_dlist_pop_front(a, &list, &v); SCL_EXPECT_EQ_I(tr, v, i);
        scl_dlist_pop_back(a, &list, &v); SCL_EXPECT_EQ_I(tr, v, i);
    }
    scl_dlist_destroy(a, &list);
}

static void test_insert_remove_at(scl_test_runner_t *tr)
{
    scl_test_group("insert at and remove at");
    scl_allocator_t *a = scl_allocator_default();
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));
    for (int i = 0; i < 5; i++) scl_dlist_push_back(a, &list, &i);
    int v = 99; SCL_EXPECT_OK(tr, scl_dlist_insert_at(a, &list, 2, &v));
    scl_dlist_remove_at(a, &list, 2, &v); SCL_EXPECT_EQ_I(tr, v, 99);
    scl_dlist_destroy(a, &list);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_dlist_t list;
    scl_dlist_init(&list, sizeof(int));
    for (int i = 0; i < 10; i++) scl_dlist_push_back(a, &list, &i);
    int key = 5, out;
    SCL_EXPECT_OK(tr, scl_dlist_search(&list, &key, cmp_int, &out));
    SCL_EXPECT_EQ_I(tr, out, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_dlist_search(&list, &key, cmp_int, &out), SCL_ERR_NOT_FOUND);
    scl_dlist_destroy(a, &list);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_dlist tests ===");
    test_init_destroy(&tr);
    test_push_pop(&tr);
    test_insert_remove_at(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
