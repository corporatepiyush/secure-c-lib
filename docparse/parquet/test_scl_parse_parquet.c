#include "../../testlib/scl_test.h"
#include "scl_parse_parquet.h"

static void create_minimal_parquet(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    fwrite("PAR1", 1, 4, f);

    fseek(f, 0, SEEK_END);
    long pos = ftell(f);

    unsigned char empty_meta[] = {0x00};
    fwrite(empty_meta, 1, 1, f);

    long after = ftell(f);
    unsigned int flen = (unsigned int)(after - pos);
    fwrite(&flen, 4, 1, f);
    fwrite("PAR1", 1, 4, f);

    fclose(f);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("file not found");
    {
        scl_parse_parquet_t pq;
        scl_error_t e = scl_parse_parquet_open(alloc, &pq, "/tmp/nonexistent.parquet");
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_NOT_FOUND);
    }

    scl_test_group("invalid file (no PAR1 magic)");
    {
        const char *path = "/tmp/test_not_parquet.bin";
        FILE *f = fopen(path, "wb");
        if (f) { fwrite("BLAH", 1, 4, f); fclose(f); }
        scl_parse_parquet_t pq;
        scl_error_t e = scl_parse_parquet_open(alloc, &pq, path);
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_INVALID_ARG);
        remove(path);
    }

    scl_test_group("open minimal parquet");
    {
        const char *path = "/tmp/test_scl_parquet.parquet";
        create_minimal_parquet(path);
        scl_parse_parquet_t pq;
        scl_error_t e = scl_parse_parquet_open(alloc, &pq, path);
        SCL_EXPECT_TRUE(&tr, e == SCL_OK);
        scl_parse_parquet_close(&pq);
        remove(path);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_parquet_open(alloc, NULL, "test.parquet") == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
