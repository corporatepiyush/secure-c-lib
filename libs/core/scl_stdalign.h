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

/* Core directive wrapper for <stdalign.h>. Single include point for alignment
 * macros. */

#ifndef SCL_STDALIGN_H
#define SCL_STDALIGN_H

#include <stdalign.h>

/*
 * scl_stdalign.h — centralized wrapper for <stdalign.h>.
 *
 * This header allows the project to:
 *   1. Ensure all stdalign.h usage flows through one include point
 *   2. Provide portable access to alignof() and _Alignas for cache-line and
 * SIMD alignment
 *   3. Future-proof against compiler-specific alignment handling
 *
 * Callers should #include <scl_stdalign.h> instead of <stdalign.h> directly.
 */

#endif // SCL_STDALIGN_H
