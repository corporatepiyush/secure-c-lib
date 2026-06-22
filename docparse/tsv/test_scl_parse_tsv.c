#include "scl_parse_tsv.h"
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
    printf("=== scl_parse_tsv tests ===\n");

    TEST("init and destroy");
    {
        scl_parse_tsv_t tsv;
        scl_error_t e = scl_parse_tsv_init(&tsv);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_parse_tsv_destroy(&tsv);
    }

    TEST("simple fields");
    {
        scl_parse_tsv_t tsv;
        scl_parse_tsv_init(&tsv);
        scl_parse_tsv_feed(&tsv, "a\tb\tc\n", 7);
        const char *f;
        size_t len;
        int ok = 1;

        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');

        if (ok) { PASS(); } else { FAIL("field parse failed"); }
        scl_parse_tsv_destroy(&tsv);
    }

    TEST("multiple rows");
    {
        scl_parse_tsv_t tsv;
        scl_parse_tsv_init(&tsv);
        scl_parse_tsv_feed(&tsv, "a\tb\nc\td\n", 9);
        const char *f;
        size_t len;
        int ok = 1;

        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_tsv_next_row(&tsv) == SCL_OK);
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'd');

        if (ok) { PASS(); } else { FAIL("multi row failed"); }
        scl_parse_tsv_destroy(&tsv);
    }

    TEST("NULL checks");
    {
        if (scl_parse_tsv_init(NULL) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
