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

/* Thread-safe fenwick data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_fenwick.h"
#include "scl_string.h"

scl_error_t scl_cfenwick_init(
    scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw, size_t n,
    size_t element_size, const void *data,
    void (*add)(void *out, const void *a, const void *SCL_RESTRICT b),
    void (*sub)(void *out, const void *a, const void *SCL_RESTRICT b)) {
  if (scl_unlikely(!fw || !add || !sub || !data))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(n == 0 || element_size == 0))
    return SCL_ERR_INVALID_ARG;

  fw->tree = scl_calloc(alloc, n + 1, element_size, alignof(max_align_t));
  fw->scratch = scl_alloc(alloc, element_size, alignof(max_align_t));
  if (scl_unlikely(!fw->tree || !fw->scratch)) {
    scl_free(alloc, fw->tree);
    scl_free(alloc, fw->scratch);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  fw->n = n;
  fw->element_size = element_size;
  fw->add = add;
  fw->sub = sub;
  scl_spinlock_init(&fw->lock);

  const unsigned char *src = data;
  for (size_t i = 1; i <= n; i++)
    scl_memcpy(fw->tree + i * element_size, src + (i - 1) * element_size,
               element_size);

  for (size_t i = 1; i <= n; i++) {
    size_t j = i + (i & -i);
    if (j <= n)
      add(fw->tree + j * element_size, fw->tree + j * element_size,
          fw->tree + i * element_size);
  }

  return SCL_OK;
}

void scl_cfenwick_destroy(scl_allocator_t *alloc,
                          scl_concurrent_fenwick_t *fw) {
  if (scl_unlikely(!fw))
    return;
  scl_free(alloc, fw->tree);
  scl_free(alloc, fw->scratch);
  fw->tree = NULL;
  fw->scratch = NULL;
  fw->n = 0;
}

scl_error_t scl_cfenwick_update(scl_allocator_t *alloc,
                                scl_concurrent_fenwick_t *fw, size_t idx,
                                const void *delta) {
  (void)alloc;
  if (scl_unlikely(!fw || !delta))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(idx >= fw->n))
    return SCL_ERR_INVALID_INDEX;

  scl_spinlock_lock(&fw->lock);

  size_t esize = fw->element_size;
  size_t i = idx + 1;

  while (i <= fw->n) {
    fw->add(fw->tree + i * esize, fw->tree + i * esize, delta);
    i += i & -i;
  }

  scl_spinlock_unlock(&fw->lock);
  return SCL_OK;
}

static scl_error_t prefix_unlocked(const scl_concurrent_fenwick_t *fw,
                                   size_t idx, void *out) {
  size_t esize = fw->element_size;
  size_t i = idx + 1;
  bool first = true;
  unsigned char *acc = out;

  while (i > 0) {
    if (first) {
      scl_memcpy(acc, fw->tree + i * esize, esize);
      first = false;
    } else {
      fw->add(acc, acc, fw->tree + i * esize);
    }
    i -= i & -i;
  }
  return first ? SCL_ERR_EMPTY : SCL_OK;
}

scl_error_t scl_cfenwick_prefix(const scl_concurrent_fenwick_t *fw, size_t idx,
                                void *SCL_RESTRICT out) {
  if (scl_unlikely(!fw || !out))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(idx >= fw->n))
    return SCL_ERR_INVALID_INDEX;

  scl_spinlock_lock((scl_spinlock_t *)&fw->lock);
  scl_error_t err = prefix_unlocked(fw, idx, out);
  scl_spinlock_unlock((scl_spinlock_t *)&fw->lock);
  return err;
}

scl_error_t scl_cfenwick_range_query(const scl_concurrent_fenwick_t *fw,
                                     size_t l, size_t r,
                                     void *SCL_RESTRICT out) {
  if (scl_unlikely(!fw || !out))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(l > r || r >= fw->n))
    return SCL_ERR_INVALID_ARG;

  scl_spinlock_lock((scl_spinlock_t *)&fw->lock);

  scl_error_t err = prefix_unlocked(fw, r, out);
  if (err != SCL_OK) {
    scl_spinlock_unlock((scl_spinlock_t *)&fw->lock);
    return err;
  }

  if (l > 0) {
    err = prefix_unlocked(fw, l - 1, fw->scratch);
    if (err != SCL_OK) {
      scl_spinlock_unlock((scl_spinlock_t *)&fw->lock);
      return err;
    }
    fw->sub(out, out, fw->scratch);
  }

  scl_spinlock_unlock((scl_spinlock_t *)&fw->lock);
  return SCL_OK;
}
