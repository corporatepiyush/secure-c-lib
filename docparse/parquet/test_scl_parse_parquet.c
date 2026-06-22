#include "scl_parse_parquet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void create_minimal_parquet(const char *path) {
    /* Create minimal Parquet file with PAR1 magic + minimal footer */
    FILE *f = fopen(path, "wb");
    if (!f) return;

    /* Header magic */
    fwrite("PAR1", 1, 4, f);

    /* Minimal metadata (footer) - empty schema, 0 rows */
    /* We'll write a valid enough structure to pass header check */
    /* Placeholder: just write PAR1 magic at both ends */
    fseek(f, 0, SEEK_END);
    long pos = ftell(f);

    /* Minimal Thrift metadata: empty struct (stop byte) */
    unsigned char empty_meta[] = {0x00};
    fwrite(empty_meta, 1, 1, f);

    /* Footer length (4 bytes) */
    long after = ftell(f);
    unsigned int flen = (unsigned int)(after - pos);
    fwrite(&flen, 4, 1, f);
    fwrite("PAR1", 1, 4, f);

    fclose(f);
}

int main(void) {
    printf("=== scl_parse_parquet tests ===\n");

    TEST("file not found");
    {
        scl_parse_parquet_t pq;
        scl_error_t e = scl_parse_parquet_open(&pq, "/tmp/nonexistent.parquet");
        if (e == SCL_ERR_NOT_FOUND) { PASS(); }
        else { FAIL("expected NOT_FOUND"); }
    }

    TEST("invalid file (no PAR1 magic)");
    {
        const char *path = "/tmp/test_not_parquet.bin";
        FILE *f = fopen(path, "wb");
        if (f) { fwrite("BLAH", 1, 4, f); fclose(f); }
        scl_parse_parquet_t pq;
        scl_error_t e = scl_parse_parquet_open(&pq, path);
        if (e == SCL_ERR_INVALID_ARG) { PASS(); }
        else { FAIL("expected INVALID_ARG"); }
        remove(path);
    }

    TEST("open minimal parquet");
    {
        const char *path = "/tmp/test_scl_parquet.parquet";
        create_minimal_parquet(path);
        scl_parse_parquet_t pq;
        scl_error_t e = scl_parse_parquet_open(&pq, path);
        if (e == SCL_OK) { PASS(); } else { FAIL("open failed"); }
        scl_parse_parquet_close(&pq);
        remove(path);
    }

    TEST("NULL checks");
    {
        if (scl_parse_parquet_open(NULL, "test.parquet") == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
