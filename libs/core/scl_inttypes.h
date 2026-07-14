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

/* Core directive wrapper for <inttypes.h>. Single include point for printf
 * format macros and int conversion. */

#ifndef SCL_INTTYPES_H
#define SCL_INTTYPES_H

#include <inttypes.h>

/*
 * scl_inttypes.h — centralized wrapper for <inttypes.h>.
 *
 * This header allows the project to:
 *   1. Ensure all inttypes.h usage flows through one include point
 *   2. Provide portable access to printf format macros (PRId64, etc.)
 *   3. Ensure consistent integer conversion across platforms
 *
 * Callers should #include <scl_inttypes.h> instead of <inttypes.h> directly.
 */

#endif // SCL_INTTYPES_H
