/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef __CUTILS_BITOPS_H
#define __CUTILS_BITOPS_H

#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#  define __builtin_popcountll __popcnt64
#endif

#ifdef __cplusplus
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS extern "C" {
#endif
#else
#define __BEGIN_DECLS
#endif

#ifdef __cplusplus
#ifndef __END_DECLS
#define __END_DECLS }
#endif
#else
#define __END_DECLS
#endif

__BEGIN_DECLS

static inline int popcount(unsigned int x) {
    return __builtin_popcount(x);
}

static inline int popcountl(unsigned long x) {
    return __builtin_popcount(x);
}

static inline int popcountll(unsigned long long x) {
    return __builtin_popcountll(x);
}

__END_DECLS

#endif /* __CUTILS_BITOPS_H */
