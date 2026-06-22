#include "../../testlib/scl_test.h"
#include "scl_parse_icelake.h"
#include <sys/stat.h>
#include <unistd.h>

static void create_metadata_json(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fwrite("{\"snapshot-id\":\"42\",\"manifests\":[{\"manifest-path\":\"s3://bucket/m1.avro\"}]"
           ",\"entries\":[{\"file-path\":\"s3://bucket/d1.parquet\"}]}", 1, 107, f);
    fclose(f);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("file not found");
    {
        scl_parse_icelake_t il;
        scl_error_t e = scl_parse_icelake_open(alloc, &il, "/tmp/nonexistent_ice");
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_NOT_FOUND);
    }

    scl_test_group("open minimal icelake");
    {
        const char *dir = "/tmp/test_ice";
        mkdir(dir, 0755);
        char meta_path[256];
        snprintf(meta_path, sizeof(meta_path), "%s/metadata.json", dir);
        create_metadata_json(meta_path);

        scl_parse_icelake_t il;
        scl_error_t e = scl_parse_icelake_open(alloc, &il, dir);
        SCL_EXPECT_TRUE(&tr, e == SCL_OK);

        int64_t sid;
        scl_parse_icelake_get_snapshot_id(&il, &sid);
        SCL_EXPECT_EQ_I(&tr, sid, 42);

        SCL_EXPECT_TRUE(&tr, il.manifest_files != NULL);

        scl_parse_icelake_close(&il);

        remove(meta_path);
        rmdir(dir);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_icelake_open(alloc, NULL, "test") == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
