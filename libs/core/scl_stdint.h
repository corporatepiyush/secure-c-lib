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

/* Core directive wrapper for <stdint.h>. Single include point for fixed-width
 * integer types. */

#ifndef SCL_STDINT_H
#define SCL_STDINT_H

#include <stdint.h>

/*
 * scl_stdint.h — centralized wrapper for <stdint.h>.
 *
 * This header allows the project to:
 *   1. Ensure all stdint.h usage flows through one include point
 *   2. Provide uniform access to fixed-width integer types (uint8_t, int64_t,
 * etc.)
 *   3. Future-proof against platform-specific integer handling
 *
 * Callers should #include <scl_stdint.h> instead of <stdint.h> directly.
 */

#endif // SCL_STDINT_H
