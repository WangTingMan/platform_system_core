/*
 * Copyright (C) 2005 The Android Open Source Project
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

//
// Handy utility functions and portability code.
//
#ifndef _LIBS_UTILS_MISC_H
#define _LIBS_UTILS_MISC_H

#include <utils/Endian.h>

#include <utils/utils_export.h>

#include <string>
#include <vector>

/* get #of elements in a static array
 * DO NOT USE: please use std::vector/std::array instead
 */
#ifndef NELEM
# define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

namespace android {

typedef void (*sysprop_change_callback)(void);
UTILS_EXPORT void add_sysprop_change_callback(sysprop_change_callback cb, int priority);
UTILS_EXPORT void report_sysprop_change();
UTILS_EXPORT std::string binary_to_hex_string(const void* a_buffer, uint32_t a_size);
inline std::string binary_to_hex_string(std::vector<char> a_buffer)
{
    return binary_to_hex_string( a_buffer.data(), a_buffer.size() );
}
}  // namespace android

#endif // _LIBS_UTILS_MISC_H
