#include "../../testlib/scl_test.h"
#include "scl_sort_radix_sort.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();
    scl_test_group("radix_sort");

    int32_t data[] = {170, 45, 75, 90, 2, 24, 802, 66};
    SCL_EXPECT_OK(&tr, scl_sort_radix_sort(a, data, 8));
    SCL_EXPECT_EQ_I(&tr, 2, data[0]);
    SCL_EXPECT_EQ_I(&tr, 802, data[7]);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
