/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* UUID v4 generation (122 random bits). Format/parse to/from 36-char hex string. Version/variant bits set per RFC 4122. */

#ifndef SCL_RAND_UUID_H
#define SCL_RAND_UUID_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef struct {
    uint8_t bytes[16];
} scl_rand_uuid_t;

scl_error_t scl_rand_uuid_generate(scl_rand_uuid_t * uuid) SCL_WARN_UNUSED;
scl_error_t scl_rand_uuid_to_string(const scl_rand_uuid_t * uuid, char * out) SCL_WARN_UNUSED;
scl_error_t scl_rand_uuid_from_string(const char * str, scl_rand_uuid_t * uuid) SCL_WARN_UNUSED;
SCL_PURE int scl_rand_uuid_compare(const scl_rand_uuid_t *a, const scl_rand_uuid_t *b);
SCL_PURE bool scl_rand_uuid_is_nil(const scl_rand_uuid_t *uuid);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
