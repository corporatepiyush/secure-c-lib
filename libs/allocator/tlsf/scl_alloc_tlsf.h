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

/* TLSF real-time allocator. 32 FL x 32 SL bins indexed via bitmap. Worst-case
 * O(1) alloc/free. Suitable for hard real-time systems. */

#ifndef SCL_ALLOC_TLSF_H
#define SCL_ALLOC_TLSF_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_allocator_t *scl_alloc_tlsf_create(scl_allocator_t *backing,
                                        size_t memory_size,
                                        size_t alignment) SCL_WARN_UNUSED;
void scl_alloc_tlsf_destroy(scl_allocator_t *alloc);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
