#include "scl_parse_csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

int main(void) {
    printf("=== scl_parse_csv tests ===\n");

    TEST("init and destroy");
    {
        scl_parse_csv_t csv;
        scl_error_t e = scl_parse_csv_init(&csv);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_parse_csv_destroy(&csv);
    }

    TEST("simple unquoted fields");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(&csv);
        scl_parse_csv_feed(&csv, "a,b,c\n", 6);
        const char *f;
        size_t len;
        int ok = 1;

        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');

        if (ok) {
            scl_parse_csv_next_field(&csv, &f, &len);
            ok = ok && len == 1 && f[0] == 'b';

            scl_parse_csv_next_field(&csv, &f, &len);
            ok = ok && len == 1 && f[0] == 'c';
        }

        if (ok) { PASS(); } else { FAIL("field parse failed"); }
        scl_parse_csv_destroy(&csv);
    }

    TEST("quoted fields with commas");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(&csv);
        scl_parse_csv_feed(&csv, "\"hello,world\",foo\n", 19);
        const char *f;
        size_t len;
        scl_parse_csv_next_field(&csv, &f, &len);
        if (len == 11 && memcmp(f, "hello,world", 11) == 0) { PASS(); }
        else { FAIL("quoted field failed"); }
        scl_parse_csv_destroy(&csv);
    }

    TEST("escaped quotes");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(&csv);
        scl_parse_csv_feed(&csv, "\"say \"\"hi\"\"\",end\n", 18);
        const char *f;
        size_t len;
        scl_parse_csv_next_field(&csv, &f, &len);
        if (len == 9 && memcmp(f, "say \"hi\"", 9) == 0) { PASS(); }
        else { FAIL("escaped quotes failed"); }
        scl_parse_csv_destroy(&csv);
    }

    TEST("multiple rows");
    {
        scl_parse_csv_t csv;
        scl_parse_csv_init(&csv);
        scl_parse_csv_feed(&csv, "a,b\nc,d\n", 8);
        const char *f;
        size_t len;
        int ok = 1;

        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_csv_next_row(&csv) == SCL_OK);
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');
        ok = ok && (scl_parse_csv_next_field(&csv, &f, &len) == SCL_OK && len == 1 && f[0] == 'd');

        if (ok) { PASS(); } else { FAIL("multi row failed"); }
        scl_parse_csv_destroy(&csv);
    }

    TEST("NULL checks");
    {
        if (scl_parse_csv_init(NULL) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
