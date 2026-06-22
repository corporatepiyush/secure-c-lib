#include "../../testlib/scl_test.h"
#include "scl_rand_uuid.h"
#include "../../stdlib/scl_stdlib.h"
#include "../../string/scl_string.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_test_group("generate_and_format");
    {
        scl_rand_uuid_t uuid;
        SCL_EXPECT_OK(&tr, scl_rand_uuid_generate(&uuid));
        char str[37];
        (void)scl_rand_uuid_to_string(&uuid, str);
        int ok = (str[8] == '-' && str[13] == '-' && str[18] == '-' && str[23] == '-');
        SCL_EXPECT_TRUE(&tr, ok);
        SCL_EXPECT_TRUE(&tr, str[14] == '4');
        char c = str[19];
        SCL_EXPECT_TRUE(&tr, c == '8' || c == '9' || c == 'a' || c == 'b' || c == 'A' || c == 'B');
    }

    scl_test_group("roundtrip");
    {
        scl_rand_uuid_t u1, u2;
        (void)scl_rand_uuid_generate(&u1);
        char str[37];
        (void)scl_rand_uuid_to_string(&u1, str);
        SCL_EXPECT_OK(&tr, scl_rand_uuid_from_string(str, &u2));
        SCL_EXPECT_EQ_I(&tr, scl_memcmp(u1.bytes, u2.bytes, 16), 0);
    }

    scl_test_group("no_duplicates");
    {
        scl_rand_uuid_t uuids[1000];
        char strs[1000][37];
        int ok = 1;
        for (int i = 0; i < 1000; i++) {
            (void)scl_rand_uuid_generate(&uuids[i]);
            (void)scl_rand_uuid_to_string(&uuids[i], strs[i]);
            for (int j = 0; j < i; j++)
                if (scl_strcmp(strs[i], strs[j]) == 0) { ok = 0; break; }
            if (!ok) break;
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("compare");
    {
        scl_rand_uuid_t a, b;
        scl_memset(&a, 0, sizeof(a));
        scl_memset(&b, 0, sizeof(b));
        SCL_EXPECT_EQ_I(&tr, scl_rand_uuid_compare(&a, &b), 0);
        b.bytes[0] = 1;
        SCL_EXPECT_TRUE(&tr, scl_rand_uuid_compare(&a, &b) < 0);
    }

    scl_test_group("is_nil");
    {
        scl_rand_uuid_t uuid;
        scl_memset(&uuid, 0, sizeof(uuid));
        SCL_EXPECT_TRUE(&tr, scl_rand_uuid_is_nil(&uuid));
        (void)scl_rand_uuid_generate(&uuid);
        SCL_EXPECT_FALSE(&tr, scl_rand_uuid_is_nil(&uuid));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
