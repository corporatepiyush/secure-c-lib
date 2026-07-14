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

/* UUID v4 generation (122 random bits). Format/parse to/from 36-char hex
 * string. Version/variant bits set per RFC 4122. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_rand_uuid.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#if defined(_WIN32)
#include <bcrypt.h>
#include <windows.h>
#elif defined(__APPLE__)
#include "scl_stdlib.h"
#else
#include <sys/random.h>
#endif

static void get_random_bytes(void *buf, size_t len) {
#if defined(_WIN32)
  BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#elif defined(__APPLE__)
  arc4random_buf(buf, len);
#else
  getrandom(buf, len, 0);
#endif
}

scl_error_t scl_rand_uuid_generate(scl_rand_uuid_t *uuid) {
  if (scl_unlikely(!uuid))
    return SCL_ERR_NULL_PTR;
  get_random_bytes(uuid->bytes, 16);
  uuid->bytes[6] = 0x40 | (uuid->bytes[6] & 0x0f);
  uuid->bytes[8] = 0x80 | (uuid->bytes[8] & 0x3f);
  return SCL_OK;
}

static const char hex_chars[] = "0123456789abcdef";

scl_error_t scl_rand_uuid_to_string(const scl_rand_uuid_t *uuid, char *out) {
  if (scl_unlikely(!uuid || !out))
    return SCL_ERR_NULL_PTR;
  int pos = 0;
  for (int i = 0; i < 16; i++) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      out[pos++] = '-';
    out[pos++] = hex_chars[(uuid->bytes[i] >> 4) & 0xf];
    out[pos++] = hex_chars[uuid->bytes[i] & 0xf];
  }
  out[36] = '\0';
  return SCL_OK;
}

static SCL_ALWAYS_INLINE SCL_PURE int hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

scl_error_t scl_rand_uuid_from_string(const char *str, scl_rand_uuid_t *uuid) {
  if (scl_unlikely(!str || !uuid))
    return SCL_ERR_NULL_PTR;
  int si = 0;
  for (int i = 0; i < 16; i++) {
    if (i == 4 || i == 6 || i == 8 || i == 10) {
      if (scl_unlikely(str[si++] != '-'))
        return SCL_ERR_INVALID_ARG;
    }
    int hi = hex_val(str[si++]);
    int lo = hex_val(str[si++]);
    if (scl_unlikely(hi < 0 || lo < 0))
      return SCL_ERR_INVALID_ARG;
    uuid->bytes[i] = (uint8_t)((hi << 4) | lo);
  }
  return SCL_OK;
}

int scl_rand_uuid_compare(const scl_rand_uuid_t *a, const scl_rand_uuid_t *b) {
  if (scl_unlikely(!a && !b))
    return 0;
  if (scl_unlikely(!a))
    return -1;
  if (scl_unlikely(!b))
    return 1;
  return scl_memcmp(a->bytes, b->bytes, 16);
}

SCL_PURE bool scl_rand_uuid_is_nil(const scl_rand_uuid_t *uuid) {
  if (scl_unlikely(!uuid))
    return true;
  for (int i = 0; i < 16; i++)
    if (uuid->bytes[i])
      return false;
  return true;
}
