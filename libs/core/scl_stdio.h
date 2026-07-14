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

/* Core directive wrapper for <stdio.h>. Single include point for all stdio
 * functionality. */

#ifndef SCL_STDIO_H
#define SCL_STDIO_H

#include <stdio.h>

/*
 * scl_stdio.h — centralized wrapper for <stdio.h>.
 *
 * This header allows the project to:
 *   1. Ensure all stdio usage flows through one include point
 *   2. Apply OS/platform-specific guards uniformly
 *   3. Future-proof stdio-related security mitigations
 *
 * Callers should #include <scl_stdio.h> instead of <stdio.h> directly.
 */

#endif // SCL_STDIO_H
