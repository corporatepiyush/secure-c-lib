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

/* sort/scl_sort.h module. */

#ifndef SCL_SORT_H
#define SCL_SORT_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

scl_error_t scl_sort_quick(void * base, size_t count, size_t element_size,
                           scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_merge(scl_allocator_t *alloc, void * base, size_t count, size_t element_size,
                           scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_heap(void * base, size_t count, size_t element_size,
                           scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_insertion(void * base, size_t count, size_t element_size,
                                scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_selection(void * base, size_t count, size_t element_size,
                                scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_bubble(void * base, size_t count, size_t element_size,
                             scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_shell(void * base, size_t count, size_t element_size,
                            scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_counting(scl_allocator_t *alloc, int32_t * base, size_t count) SCL_WARN_UNUSED;
scl_error_t scl_sort_radix(scl_allocator_t *alloc, int32_t * base, size_t count) SCL_WARN_UNUSED;
scl_error_t scl_sort_bucket(scl_allocator_t *alloc, void * base, size_t count, size_t element_size,
                             scl_cmp_func_t cmp) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
