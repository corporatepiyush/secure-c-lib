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

/* Core directive wrapper for <errno.h>. Single include point for all error
 * codes. */

#ifndef SCL_ERRNO_H
#define SCL_ERRNO_H

#include <errno.h>

/*
 * scl_errno.h — centralized wrapper for <errno.h>.
 *
 * This header allows the project to:
 *   1. Ensure all errno usage flows through one include point
 *   2. Apply OS-specific error code handling uniformly
 *   3. Future-proof errno-related error handling strategies
 *
 * Callers should #include <scl_errno.h> instead of <errno.h> directly.
 */

#endif // SCL_ERRNO_H
