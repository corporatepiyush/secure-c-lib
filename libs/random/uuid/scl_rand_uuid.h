#ifndef SCL_RAND_UUID_H
#define SCL_RAND_UUID_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"

typedef struct {
    uint8_t bytes[16];
} scl_rand_uuid_t;

scl_error_t scl_rand_uuid_generate(scl_rand_uuid_t *uuid) SCL_WARN_UNUSED;
scl_error_t scl_rand_uuid_to_string(const scl_rand_uuid_t *uuid, char out[37]) SCL_WARN_UNUSED;
scl_error_t scl_rand_uuid_from_string(const char str[36], scl_rand_uuid_t *uuid) SCL_WARN_UNUSED;
int scl_rand_uuid_compare(const scl_rand_uuid_t *a, const scl_rand_uuid_t *b);
bool scl_rand_uuid_is_nil(const scl_rand_uuid_t *uuid);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
