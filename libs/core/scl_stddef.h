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

/* Core directive wrapper for <stddef.h>. Single include point for size_t and
 * NULL. */

#ifndef SCL_STDDEF_H
#define SCL_STDDEF_H

#include <stddef.h>

/*
 * scl_stddef.h — centralized wrapper for <stddef.h>.
 *
 * This header allows the project to:
 *   1. Ensure all stddef.h usage flows through one include point
 *   2. Provide portable access to size_t, NULL, and ptrdiff_t
 *
 * Callers should #include <scl_stddef.h> instead of <stddef.h> directly.
 */

#endif // SCL_STDDEF_H
