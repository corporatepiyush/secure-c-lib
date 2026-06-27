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

/* Spinlock type (scl_spinlock_t) with TTAS-and-pause backoff, cache-line alignment/padding macros for false-sharing prevention, memory-order shortcuts for concurrent data structures. */

#include "scl_concurrent_common.h"

/* Currently all helpers are inline in header;
   this file is a placeholder for future out-of-line helpers. */
