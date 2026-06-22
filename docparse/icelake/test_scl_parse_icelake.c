#include "scl_parse_icelake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void create_minimal_iceberg_v2(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    const char *json = "{"
        "\"format-version\": 2,"
        "\"table-uuid\": \"abc123\","
        "\"location\": \"/tmp/table\","
        "\"last-sequence-number\": 1,"
        "\"last-updated-ms\": 1000,"
        "\"current-snapshot-id\": 42,"
        "\"snapshots\": [{\"snapshot-id\": 42, \"timestamp-ms\": 1000}],"
        "\"schemas\": [{\"schema-id\": 0, \"type\": \"struct\", \"fields\": [{\"id\": 1, \"name\": \"col1\", \"type\": \"int\"}]}],"
        "\"default-spec-id\": 0,"
        "\"partition-specs\": [],"
        "\"last-partition-id\": 0"
        "}";
    fwrite(json, 1, strlen(json), f);
    fclose(f);
}

int main(void) {
    printf("=== scl_parse_icelake tests ===\n");

    TEST("file not found");
    {
        scl_parse_icelake_t ice;
        scl_error_t e = scl_parse_icelake_open(&ice, "/tmp/nonexistent.metadata.json");
        if (e == SCL_ERR_NOT_FOUND) { PASS(); }
        else { FAIL("expected NOT_FOUND"); }
    }

    TEST("open minimal v2 metadata");
    {
        const char *path = "/tmp/test_iceberg_v2.metadata.json";
        create_minimal_iceberg_v2(path);
        scl_parse_icelake_t ice;
        scl_error_t e = scl_parse_icelake_open(&ice, path);
        if (e == SCL_OK) {
            int snap_count = 0;
            scl_parse_icelake_get_snapshot_count(&ice, &snap_count);
            if (snap_count == 1) { PASS(); }
            else { FAIL("snapshot count mismatch"); }
        } else { FAIL("open failed"); }
        scl_parse_icelake_close(&ice);
        remove(path);
    }

    TEST("get schema");
    {
        const char *path = "/tmp/test_iceberg_schema.metadata.json";
        create_minimal_iceberg_v2(path);
        scl_parse_icelake_t ice;
        scl_parse_icelake_open(&ice, path);
        const char *schema = NULL;
        size_t slen = 0;
        scl_error_t e = scl_parse_icelake_get_schema(&ice, &schema, &slen);
        if (e == SCL_OK && schema && slen > 0) { PASS(); }
        else { FAIL("get schema failed"); }
        scl_parse_icelake_close(&ice);
        remove(path);
    }

    TEST("NULL checks");
    {
        if (scl_parse_icelake_open(NULL, "test.json") == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
