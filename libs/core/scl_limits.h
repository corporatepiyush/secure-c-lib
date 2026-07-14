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

/* Core directive wrapper for <limits.h>. Single include point for all integer
 * limits. */

#ifndef SCL_LIMITS_H
#define SCL_LIMITS_H

#include <limits.h>

/*
 * scl_limits.h — centralized wrapper for <limits.h>.
 *
 * This header allows the project to:
 *   1. Ensure all limits.h usage flows through one include point
 *   2. Apply OS/architecture-specific limit guards uniformly
 *   3. Provide portable access to integer limits across platforms
 *
 * Callers should #include <scl_limits.h> instead of <limits.h> directly.
 */

#endif // SCL_LIMITS_H
