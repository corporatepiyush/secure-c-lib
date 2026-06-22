#include "scl_bloom.h"
#include "../testlib/scl_test.h"
#include <string.h>

static void test_insert_contains(scl_test_runner_t *tr)
{
    scl_test_group("insert and maybe contains");
    scl_allocator_t *a = scl_allocator_default();
    scl_bloom_t bf;
    scl_bloom_init(a, &bf, 100, 0.01, scl_bloom_hash_murmur);
    const char *key = "test_key";
    SCL_EXPECT_OK(tr, scl_bloom_insert(&bf, key, strlen(key)));
    SCL_EXPECT_TRUE(tr, scl_bloom_maybe_contains(&bf, key, strlen(key)));
    SCL_EXPECT_FALSE(tr, scl_bloom_maybe_contains(&bf, "not_inserted", 12));
    scl_bloom_destroy(a, &bf);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_bloom tests ===");
    test_insert_contains(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
