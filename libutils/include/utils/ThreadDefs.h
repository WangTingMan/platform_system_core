/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef _LIBS_UTILS_THREAD_DEFS_H
#define _LIBS_UTILS_THREAD_DEFS_H

#include <stdint.h>
#include <sys/types.h>

// ---------------------------------------------------------------------------
// C API

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef uint32_t android_thread_id_t;
#else
typedef void* android_thread_id_t;
#endif

typedef int (*android_thread_func_t)(void*);

#ifdef __cplusplus
} // extern "C"
#endif

// ---------------------------------------------------------------------------
// C++ API
#ifdef __cplusplus
namespace android {
// ---------------------------------------------------------------------------

typedef android_thread_id_t thread_id_t;
typedef android_thread_func_t thread_func_t;

enum {
    PRIORITY_LOWEST         = 0,
    PRIORITY_BACKGROUND     = 1,
    PRIORITY_NORMAL         = 2,
    PRIORITY_FOREGROUND     = 3,
    PRIORITY_DISPLAY        = 4,
    PRIORITY_URGENT_DISPLAY = 5,
    PRIORITY_AUDIO          = 6,
    PRIORITY_URGENT_AUDIO   = 7,
    PRIORITY_HIGHEST        = 8,
    PRIORITY_DEFAULT        = 9,
    PRIORITY_MORE_FAVORABLE = 10,
    PRIORITY_LESS_FAVORABLE = 11,
};

// ---------------------------------------------------------------------------
}  // namespace android
#endif  // __cplusplus
// ---------------------------------------------------------------------------

#endif // _LIBS_UTILS_THREAD_DEFS_H
