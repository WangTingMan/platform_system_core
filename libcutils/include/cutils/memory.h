/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>
#include "cutils/cutils_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GLIBC__) || defined(_WIN32)
/* Declaration of strlcpy() for platforms that don't already have it. */
CUTILS_EXPORT size_t strlcpy(char *dst, const char *src, size_t size);

CUTILS_EXPORT char* strtok_r(char* str, const char* delim, char** saveptr);

CUTILS_EXPORT size_t strlcat(char* dest, const char* src, size_t destsz);

#endif

#ifdef __cplusplus
} // extern "C"
#endif
