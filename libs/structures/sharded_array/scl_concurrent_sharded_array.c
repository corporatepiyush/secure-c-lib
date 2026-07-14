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

#include "scl_concurrent_sharded_array.h"
#include "scl_string.h"

scl_error_t scl_csa_init(scl_allocator_t *alloc,
                         scl_concurrent_sharded_array_t *csa, size_t elem_size,
                         size_t shard_len) {
  if (scl_unlikely(!csa))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(elem_size == 0))
    return SCL_ERR_INVALID_ARG;
  if (shard_len == 0)
    shard_len = SCL_CONCURRENT_SHARDED_ARRAY_DEFAULT_SHARD;

  size_t probe;
  if (scl_unlikely(scl_mul_overflow(shard_len, elem_size, &probe)))
    return SCL_ERR_SIZE_OVERFLOW;

  csa->alloc = alloc;
  csa->head = NULL;
  csa->tail = NULL;
  csa->elem_size = elem_size;
  csa->shard_len = shard_len;
  csa->count = 0;
  csa->shard_count = 0;
  scl_spinlock_init(&csa->lock);
  return SCL_OK;
}

scl_error_t scl_csa_append(scl_allocator_t *alloc,
                           scl_concurrent_sharded_array_t *csa,
                           const void *elem, size_t *out_index) {
  if (scl_unlikely(!csa || !elem))
    return SCL_ERR_NULL_PTR;

  scl_spinlock_lock(&csa->lock);

  size_t idx = csa->count;
  size_t off = idx % csa->shard_len;

  /* Need a new shard? */
  if (off == 0) {
    size_t sbytes = csa->shard_len * csa->elem_size;
    scsa_shard_t *sh = (scsa_shard_t *)scl_alloc(
        alloc, sizeof(scsa_shard_t) + sbytes, alignof(max_align_t));
    if (scl_unlikely(!sh)) {
      scl_spinlock_unlock(&csa->lock);
      return SCL_ERR_OUT_OF_MEMORY;
    }
    sh->next = NULL;
    if (csa->tail)
      csa->tail->next = sh;
    else
      csa->head = sh;
    csa->tail = sh;
    csa->shard_count++;
  }

  unsigned char *slot = csa->tail->data + off * csa->elem_size;
  scl_memcpy(slot, elem, csa->elem_size);
  csa->count++;

  scl_spinlock_unlock(&csa->lock);

  if (out_index)
    *out_index = idx;
  return SCL_OK;
}

void *scl_csa_get(const scl_concurrent_sharded_array_t *csa, size_t index) {
  if (scl_unlikely(!csa || index >= csa->count))
    return NULL;
  size_t shard_idx = index / csa->shard_len;
  size_t off = index % csa->shard_len;
  scsa_shard_t *sh = csa->head;
  for (size_t i = 0; i < shard_idx; i++)
    sh = sh->next;
  return sh->data + off * csa->elem_size;
}

size_t scl_csa_count(const scl_concurrent_sharded_array_t *csa) {
  return csa ? csa->count : 0;
}

void scl_csa_destroy(scl_allocator_t *alloc,
                     scl_concurrent_sharded_array_t *csa) {
  if (!csa)
    return;
  scsa_shard_t *sh = csa->head;
  while (sh) {
    scsa_shard_t *next = sh->next;
    scl_free(alloc, sh);
    sh = next;
  }
  csa->head = NULL;
  csa->tail = NULL;
  csa->count = csa->shard_count = 0;
}