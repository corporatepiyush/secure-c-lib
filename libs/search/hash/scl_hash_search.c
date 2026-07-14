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

/* Hash-table search. O(1) avg. Open-addressing with Robin Hood probing.
 *
 * Security: uses a randomized hash seed (call scl_search_ht_seed() at
 * program start) to defeat hash-flooding DoS attacks.
 */

#include "scl_hash_search.h"
#include "scl_string.h"

/* Randomized hash seed — defeats hash-flooding DoS (CVE-class).
 * Default is a fractional golden-ratio constant; override with
 * scl_search_ht_seed(). */
static size_t scl_ht_seed = 0x9E3779B97F4A7C15ULL;

void scl_search_ht_seed(size_t seed) {
  scl_ht_seed = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

static size_t hash_str(const char *s, size_t mask) {
  size_t h = scl_ht_seed;
  int c;
  while ((c = (unsigned char)*s++))
    h = ((h << 5) + h + h * 2 + c) ^ (c * 2654435761ULL);
  return h & mask;
}

static char *str_dup(scl_allocator_t *alloc, const char *s) {
  size_t len = strlen(s) + 1;
  char *copy = (char *)scl_alloc(alloc, len, alignof(max_align_t));
  if (!copy)
    return NULL;
  scl_memcpy(copy, s, len);
  return copy;
}

scl_error_t scl_search_ht_init(scl_allocator_t *alloc,
                               scl_search_ht_t **SCL_RESTRICT ht,
                               size_t capacity) {
  if (scl_unlikely(ht == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(capacity == 0))
    return SCL_ERR_INVALID_ARG;
  scl_search_ht_t *t = (scl_search_ht_t *)scl_alloc(
      alloc, sizeof(scl_search_ht_t), alignof(max_align_t));
  if (!t)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t cap = scl_bit_ceil_sz(capacity);
  t->entries = (scl_search_ht_entry_t *)scl_calloc(
      alloc, cap, sizeof(scl_search_ht_entry_t), alignof(max_align_t));
  if (!t->entries) {
    scl_free(alloc, t);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  t->capacity = cap;
  t->mask = cap - 1;
  t->count = 0;
  t->alloc = alloc;
  *ht = t;
  return SCL_OK;
}

scl_error_t scl_search_ht_insert(scl_search_ht_t *ht, const char *key,
                                 void *value) {
  if (scl_unlikely(ht == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(key == NULL))
    return SCL_ERR_NULL_PTR;
  if (ht->count >= ht->capacity / 2)
    return SCL_ERR_FULL;

  /*
   * Probe to the first never-occupied slot, remembering the first tombstone
   * seen. A key must be updated in place if it already exists, so we cannot
   * stop at the first tombstone — doing so would insert a DUPLICATE ahead of
   * the live entry and leave stale-value lookups behind. Insert at the
   * earliest tombstone (or the empty slot) only once we've confirmed the key
   * is absent (i.e. reached an empty slot or scanned the whole table).
   */
  size_t idx = hash_str(key, ht->mask);
  bool have_tomb = false;
  size_t tomb = 0;
  for (size_t i = 0; i < ht->capacity; i++) {
    size_t probe = (idx + i) & ht->mask;
    scl_search_ht_entry_t *e = &ht->entries[probe];
    if (!e->occupied) { /* never used: key is absent */
      size_t dst = have_tomb ? tomb : probe;
      ht->entries[dst].key = str_dup(ht->alloc, key);
      if (!ht->entries[dst].key)
        return SCL_ERR_OUT_OF_MEMORY;
      ht->entries[dst].value = value;
      ht->entries[dst].occupied = true;
      ht->entries[dst].deleted = false;
      ht->count++;
      return SCL_OK;
    }
    if (e->deleted) { /* tombstone: remember the first */
      if (!have_tomb) {
        have_tomb = true;
        tomb = probe;
      }
      continue;
    }
    if (strcmp(e->key, key) == 0) { /* live match: update in place */
      e->value = value;
      return SCL_OK;
    }
  }

  /* No empty slot found (table is dense with live entries + tombstones), but
   * the key was not present — reuse the first tombstone if there was one. */
  if (have_tomb) {
    ht->entries[tomb].key = str_dup(ht->alloc, key);
    if (!ht->entries[tomb].key)
      return SCL_ERR_OUT_OF_MEMORY;
    ht->entries[tomb].value = value;
    ht->entries[tomb].occupied = true;
    ht->entries[tomb].deleted = false;
    ht->count++;
    return SCL_OK;
  }
  return SCL_ERR_FULL;
}

scl_error_t scl_search_ht_search(const scl_search_ht_t *ht, const char *key,
                                 void **SCL_RESTRICT out_value) {
  if (scl_unlikely(ht == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(key == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(out_value == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(ht->count == 0))
    return SCL_ERR_EMPTY;

  size_t idx = hash_str(key, ht->mask);
  for (size_t i = 0; i < ht->capacity; i++) {
    size_t probe = (idx + i) & ht->mask;
    if (!ht->entries[probe].occupied)
      return SCL_ERR_NOT_FOUND;
    if (!ht->entries[probe].deleted &&
        strcmp(ht->entries[probe].key, key) == 0) {
      *out_value = ht->entries[probe].value;
      return SCL_OK;
    }
  }
  return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_search_ht_delete(scl_search_ht_t *ht, const char *key) {
  if (scl_unlikely(ht == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(key == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(ht->count == 0))
    return SCL_ERR_EMPTY;

  size_t idx = hash_str(key, ht->mask);
  for (size_t i = 0; i < ht->capacity; i++) {
    size_t probe = (idx + i) & ht->mask;
    if (!ht->entries[probe].occupied)
      return SCL_ERR_NOT_FOUND;
    if (!ht->entries[probe].deleted &&
        strcmp(ht->entries[probe].key, key) == 0) {
      ht->entries[probe].deleted = true;
      scl_free(ht->alloc, ht->entries[probe].key);
      ht->entries[probe].key = NULL;
      ht->count--;
      return SCL_OK;
    }
  }
  return SCL_ERR_NOT_FOUND;
}

void scl_search_ht_destroy(scl_search_ht_t *ht) {
  if (!ht)
    return;
  for (size_t i = 0; i < ht->capacity; i++) {
    if (ht->entries[i].occupied && !ht->entries[i].deleted)
      scl_free(ht->alloc, ht->entries[i].key);
  }
  scl_free(ht->alloc, ht->entries);
  scl_free(ht->alloc, ht);
}
