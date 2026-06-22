#include "scl_parse_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

int main(void) {
    printf("=== scl_parse_json tests ===\n");

    TEST("parse null");
    {
        scl_parse_json_value_t *root = NULL;
        scl_error_t e = scl_parse_json_parse("null", &root);
        if (e == SCL_OK && root && scl_parse_json_get_type(root) == SCL_JSON_NULL) {
            PASS();
        } else { FAIL("null parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("parse bool true");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("true", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_BOOL && scl_parse_json_get_bool(root)) {
            PASS();
        } else { FAIL("true parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("parse integer");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("42", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_INT64 && scl_parse_json_get_int(root) == 42) {
            PASS();
        } else { FAIL("int parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("parse double");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("3.14", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_DOUBLE && fabs(scl_parse_json_get_double(root) - 3.14) < 0.001) {
            PASS();
        } else { FAIL("double parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("parse string");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("\"hello\"", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_STRING &&
            strcmp(scl_parse_json_get_string(root), "hello") == 0) {
            PASS();
        } else { FAIL("string parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("parse array");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("[1,2,3]", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_ARRAY &&
            scl_parse_json_array_len(root) == 3 &&
            scl_parse_json_get_int(scl_parse_json_array_get(root, 0)) == 1 &&
            scl_parse_json_get_int(scl_parse_json_array_get(root, 1)) == 2 &&
            scl_parse_json_get_int(scl_parse_json_array_get(root, 2)) == 3) {
            PASS();
        } else { FAIL("array parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("parse object");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("{\"key\":\"value\"}", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_OBJECT &&
            scl_parse_json_object_len(root) == 1) {
            scl_parse_json_value_t *v = scl_parse_json_object_get(root, "key");
            if (v && scl_parse_json_get_type(v) == SCL_JSON_STRING &&
                strcmp(scl_parse_json_get_string(v), "value") == 0) {
                PASS();
            } else { FAIL("object get failed"); }
        } else { FAIL("object parse failed"); }
        scl_parse_json_free(root);
    }

    TEST("nested object");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse("{\"a\":{\"b\":[1,2]}}", &root);
        if (!root) { FAIL("parse failed"); return 1; }
        scl_parse_json_value_t *a = scl_parse_json_object_get(root, "a");
        scl_parse_json_value_t *b = a ? scl_parse_json_object_get(a, "b") : NULL;
        scl_parse_json_value_t *first = b ? scl_parse_json_array_get(b, 0) : NULL;
        if (first && scl_parse_json_get_int(first) == 1) { PASS(); }
        else { FAIL("nested access failed"); }
        scl_parse_json_free(root);
    }

    TEST("NULL checks");
    {
        scl_parse_json_value_t *r;
        if (scl_parse_json_parse(NULL, &r) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
