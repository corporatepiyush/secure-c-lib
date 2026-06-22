#include "../../testlib/scl_test.h"
#include "../../string/scl_string.h"
#include <math.h>
#include "scl_parse_json.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("parse null");
    {
        scl_parse_json_value_t *root = NULL;
        scl_error_t e = scl_parse_json_parse(alloc, "null", &root);
        if (e == SCL_OK && root && scl_parse_json_get_type(root) == SCL_JSON_NULL) {
            SCL_EXPECT_TRUE(&tr, 1);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("parse bool true");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "true", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_BOOL && scl_parse_json_get_bool(root)) {
            SCL_EXPECT_TRUE(&tr, 1);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("parse integer");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "42", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_INT64 && scl_parse_json_get_int(root) == 42) {
            SCL_EXPECT_TRUE(&tr, 1);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("parse double");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "3.14", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_DOUBLE && fabs(scl_parse_json_get_double(root) - 3.14) < 0.001) {
            SCL_EXPECT_TRUE(&tr, 1);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("parse string");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "\"hello\"", &root);
        if (root && scl_parse_json_get_type(root) == SCL_JSON_STRING &&
            scl_strcmp(scl_parse_json_get_string(root), "hello") == 0) {
            SCL_EXPECT_TRUE(&tr, 1);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("parse array");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "[1,2,3]", &root);
        int ok = (root && scl_parse_json_get_type(root) == SCL_JSON_ARRAY &&
                  scl_parse_json_array_len(root) == 3 &&
                  scl_parse_json_get_int(scl_parse_json_array_get(root, 0)) == 1 &&
                  scl_parse_json_get_int(scl_parse_json_array_get(root, 1)) == 2 &&
                  scl_parse_json_get_int(scl_parse_json_array_get(root, 2)) == 3);
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("parse object");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "{\"key\":\"value\"}", &root);
        int ok = 0;
        if (root && scl_parse_json_get_type(root) == SCL_JSON_OBJECT &&
            scl_parse_json_object_len(root) == 1) {
            scl_parse_json_value_t *v = scl_parse_json_object_get(root, "key");
            if (v && scl_parse_json_get_type(v) == SCL_JSON_STRING &&
                scl_strcmp(scl_parse_json_get_string(v), "value") == 0) ok = 1;
        }
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("nested object");
    {
        scl_parse_json_value_t *root = NULL;
        scl_parse_json_parse(alloc, "{\"a\":{\"b\":[1,2]}}", &root);
        int ok = 0;
        if (root) {
            scl_parse_json_value_t *a = scl_parse_json_object_get(root, "a");
            scl_parse_json_value_t *b = a ? scl_parse_json_object_get(a, "b") : NULL;
            scl_parse_json_value_t *first = b ? scl_parse_json_array_get(b, 0) : NULL;
            if (first && scl_parse_json_get_int(first) == 1) ok = 1;
        }
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_json_free(alloc, root);
    }

    scl_test_group("NULL checks");
    {
        scl_parse_json_value_t *r;
        SCL_EXPECT_TRUE(&tr, scl_parse_json_parse(alloc, NULL, &r) == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
