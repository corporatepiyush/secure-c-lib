#include "../../testlib/scl_test.h"
#include "../../stdlib/scl_stdlib.h"
#include "scl_parse_csv.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("init and destroy");
    {
        scl_parse_csv_t csv;
        SCL_EXPECT_OK(&tr, scl_parse_csv_init(alloc, &csv));
        scl_parse_csv_destroy(&csv);
    }

    scl_test_group("simple unquoted fields");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(alloc, &csv);
        scl_parse_csv_feed(&csv, "a,b,c\n", 6);
        const char *f;
        size_t len;
        int ok = 1;
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_csv_destroy(&csv);
    }

    scl_test_group("quoted fields with commas");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(alloc, &csv);
        scl_parse_csv_feed(&csv, "\"hello,world\",foo\n", 19);
        const char *f;
        size_t len;
        scl_parse_csv_next_field(&csv, &f, &len);
        SCL_EXPECT_TRUE(&tr, len == 11 && scl_memcmp(f, "hello,world", 11) == 0);
        scl_parse_csv_destroy(&csv);
    }

    scl_test_group("escaped quotes");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(alloc, &csv);
        scl_parse_csv_feed(&csv, "\"say \"\"hi\"\"\",end\n", 18);
        const char *f;
        size_t len;
        scl_parse_csv_next_field(&csv, &f, &len);
        SCL_EXPECT_TRUE(&tr, len == 9 && scl_memcmp(f, "say \"hi\"", 9) == 0);
        scl_parse_csv_destroy(&csv);
    }

    scl_test_group("multiple rows");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(alloc, &csv);
        scl_parse_csv_feed(&csv, "a,b\nc,d\n", 8);
        const char *f;
        size_t len;
        int ok = 1;
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_csv_next_row(&csv) == SCL_OK);
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'd');
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_csv_destroy(&csv);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_csv_init(alloc, NULL) == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
