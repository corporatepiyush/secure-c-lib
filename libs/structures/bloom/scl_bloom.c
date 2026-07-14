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

/* bloom data structure. */

#include "scl_bloom.h"
#include "scl_math.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

size_t scl_bloom_hash_murmur(const void *data, size_t len, size_t seed) {
  const unsigned char *key = (const unsigned char *)data;
  size_t h = seed;
  for (size_t i = 0; i < len; i++) {
    h ^= key[i];
    h *= 0x9e3779b97f4a7c15ULL;
    h ^= h >> 47;
  }
  return h;
}

static SCL_ALWAYS_INLINE void scl_bloom_set_bit(unsigned char *bits,
                                                size_t idx) {
  bits[idx / 8] |= (1 << (idx % 8));
}

static SCL_ALWAYS_INLINE bool scl_bloom_test_bit(const unsigned char *bits,
                                                 size_t idx) {
  return (bits[idx / 8] >> (idx % 8)) & 1;
}

scl_error_t scl_bloom_init(scl_allocator_t *alloc, scl_bloom_t *bf,
                           size_t expected_items, double false_positive_rate,
                           scl_bloom_hash_t hash_func) {
  if (scl_unlikely(!bf))
    return SCL_ERR_NULL_PTR;
  if (expected_items == 0 || false_positive_rate <= 0.0 ||
      false_positive_rate >= 1.0 || !hash_func)
    return SCL_ERR_INVALID_ARG;

  double ln2 = 0.6931471805599453;
  double ln2sq = ln2 * ln2;
  double bits_d =
      -((double)expected_items * scl_log(false_positive_rate)) / ln2sq;
  if (scl_unlikely(bits_d <= 0.0 || bits_d >= (double)SIZE_MAX))
    return SCL_ERR_INVALID_ARG;
  size_t bits = (size_t)bits_d;
  if (bits == 0)
    bits = 1;

  size_t num_hashes = (size_t)(((double)bits / (double)expected_items) * ln2);
  if (num_hashes < 1)
    num_hashes = 1;
  if (num_hashes > 64)
    num_hashes = 64;

  size_t bits_padded;
  if (scl_unlikely(scl_add_overflow(bits, 7, &bits_padded)))
    return SCL_ERR_SIZE_OVERFLOW;
  size_t bytes = bits_padded / 8;
  bf->bits = scl_calloc(alloc, bytes, 1, alignof(max_align_t));
  if (scl_unlikely(!bf->bits))
    return SCL_ERR_OUT_OF_MEMORY;

  bf->bit_count = bits;
  bf->byte_count = bytes;
  bf->num_hashes = num_hashes;
  bf->hash_func = hash_func;
  bf->inserted = 0;
  return SCL_OK;
}

void scl_bloom_destroy(scl_allocator_t *alloc, scl_bloom_t *bf) {
  if (bf) {
    scl_free(alloc, bf->bits);
    bf->bits = NULL;
    bf->bit_count = 0;
    bf->byte_count = 0;
    bf->inserted = 0;
  }
}

scl_error_t scl_bloom_insert(scl_bloom_t *bf, const void *data, size_t len) {
  if (scl_unlikely(!bf || !data))
    return SCL_ERR_NULL_PTR;

  for (size_t i = 0; i < bf->num_hashes; i++) {
    size_t h = bf->hash_func(data, len, i);
    scl_bloom_set_bit(bf->bits, h % bf->bit_count);
  }
  bf->inserted++;
  return SCL_OK;
}

bool scl_bloom_maybe_contains(const scl_bloom_t *bf, const void *data,
                              size_t len) {
  if (scl_unlikely(!bf || !data))
    return false;

  for (size_t i = 0; i < bf->num_hashes; i++) {
    size_t h = bf->hash_func(data, len, i);
    if (!scl_bloom_test_bit(bf->bits, h % bf->bit_count))
      return false;
  }
  return true;
}

void scl_bloom_clear(scl_bloom_t *bf) {
  if (bf) {
    scl_memset(bf->bits, 0, bf->byte_count);
    bf->inserted = 0;
  }
}

size_t scl_bloom_count(const scl_bloom_t *bf) { return bf ? bf->inserted : 0; }

double scl_bloom_false_positive_rate(const scl_bloom_t *bf) {
  if (!bf || bf->inserted == 0)
    return 0.0;
  double e =
      -((double)bf->num_hashes * (double)bf->inserted) / (double)bf->bit_count;
  return scl_pow(1.0 - scl_exp(e), (double)bf->num_hashes);
}
