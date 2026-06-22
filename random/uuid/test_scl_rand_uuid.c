#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scl_rand_uuid.h"

static int failed = 0;

#define TEST(name) do { printf("  " name "... "); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

static void test_generate_and_format(void) {
    TEST("generate produces valid UUID format");
    scl_rand_uuid_t uuid;
    if (scl_rand_uuid_generate(&uuid) != SCL_OK) { FAIL("generate"); return; }
    char str[37];
    (void)scl_rand_uuid_to_string(&uuid, str);
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
        { FAIL("wrong dash positions"); return; }
    if (str[14] != '4') { FAIL("wrong version"); return; }
    char c = str[19];
    if (c != '8' && c != '9' && c != 'a' && c != 'b' && c != 'A' && c != 'B')
        { FAIL("wrong variant"); return; }
    for (int i = 0; i < 36; i++) {
        if (str[i] == '-') continue;
        if (!((str[i] >= '0' && str[i] <= '9') ||
              (str[i] >= 'a' && str[i] <= 'f') ||
              (str[i] >= 'A' && str[i] <= 'F')))
            { FAIL("invalid hex char"); return; }
    }
    PASS();
}

static void test_roundtrip(void) {
    TEST("parse round-trips correctly");
    scl_rand_uuid_t u1, u2;
    (void)scl_rand_uuid_generate(&u1);
    char str[37];
    (void)scl_rand_uuid_to_string(&u1, str);
    if (scl_rand_uuid_from_string(str, &u2) != SCL_OK)
        { FAIL("parse failed"); return; }
    if (memcmp(u1.bytes, u2.bytes, 16) != 0)
        { FAIL("bytes differ"); return; }
    PASS();
}

static void test_no_duplicates(void) {
    TEST("1000 UUIDs have no duplicates");
    scl_rand_uuid_t uuids[1000];
    char strs[1000][37];
    for (int i = 0; i < 1000; i++) {
        (void)scl_rand_uuid_generate(&uuids[i]);
        (void)scl_rand_uuid_to_string(&uuids[i], strs[i]);
        for (int j = 0; j < i; j++)
            if (strcmp(strs[i], strs[j]) == 0)
                { FAIL("duplicate found"); return; }
    }
    PASS();
}

static void test_compare(void) {
    TEST("compare works correctly");
    scl_rand_uuid_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    if (scl_rand_uuid_compare(&a, &b) != 0) { FAIL("equal not zero"); return; }
    b.bytes[0] = 1;
    if (scl_rand_uuid_compare(&a, &b) >= 0) { FAIL("compare order"); return; }
    PASS();
}

static void test_is_nil(void) {
    TEST("is_nil works correctly");
    scl_rand_uuid_t uuid;
    memset(&uuid, 0, sizeof(uuid));
    if (!scl_rand_uuid_is_nil(&uuid)) { FAIL("all zeros not nil"); return; }
    (void)scl_rand_uuid_generate(&uuid);
    if (scl_rand_uuid_is_nil(&uuid)) { FAIL("random uuid is nil"); return; }
    PASS();
}

int main(void) {
    printf("scl_rand_uuid tests:\n");
    test_generate_and_format();
    test_roundtrip();
    test_no_duplicates();
    test_compare();
    test_is_nil();
    printf("\n%d test(s) failed\n", failed);
    return failed ? 1 : 0;
}
