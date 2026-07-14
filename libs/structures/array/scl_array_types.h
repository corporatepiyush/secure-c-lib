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

/* Lightweight flat array type — struct only, no API.
 * Full API lives in scl_array.h; scl_graph_types.h also relies on this. */

#ifndef SCL_ARRAY_TYPES_H
#define SCL_ARRAY_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef SCL_ARRAY_TYPE_DEFINED
#define SCL_ARRAY_TYPE_DEFINED
typedef struct {
  unsigned char *data;
  size_t element_size;
  size_t capacity;
  size_t count;
} scl_array_t;
#endif

#endif