#include "scl_lru.h"
#include "../testlib/scl_test.h"
#include <string.h>

static void test_put_get(scl_test_runner_t *tr)
{
    scl_test_group("put and get");
    scl_allocator_t *a = scl_allocator_default();
    scl_lru_t cache;
    scl_lru_init(a, &cache, sizeof(int), sizeof(int), 3);
    int k, v;
    k = 1; v = 100; scl_lru_put(a, &cache, &k, &v);
    k = 2; v = 200; scl_lru_put(a, &cache, &k, &v);
    k = 3; v = 300; scl_lru_put(a, &cache, &k, &v);
    k = 1; SCL_EXPECT_OK(tr, scl_lru_get(&cache, &k, &v)); SCL_EXPECT_EQ_I(tr, v, 100);
    k = 4; v = 400; scl_lru_put(a, &cache, &k, &v);
    k = 2; SCL_EXPECT_ERROR(tr, scl_lru_get(&cache, &k, &v), SCL_ERR_NOT_FOUND);
    scl_lru_destroy(a, &cache);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_lru tests ===");
    test_put_get(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
