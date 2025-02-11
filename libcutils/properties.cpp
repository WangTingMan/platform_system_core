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

#define NOMINMAX

#include <cutils/properties.h>

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#include "internal_property.h"
#else
#include <unistd.h>
#include <android-base/properties.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

property_set_callback_type g_set_callback = &property_internal::__internal_property_set;
property_get_callback_type g_get_callback = &property_internal::__internal_property_get;

void set_property_callback( property_set_callback_type set_callback,
    property_get_callback_type get_callback )
{
    if( set_callback && get_callback )
    {
        g_set_callback = set_callback;
        g_get_callback = get_callback;
    }
}

int8_t property_get_bool(const char* key, int8_t default_value) {
    if (!key) return default_value;

    int8_t result = default_value;
    char buf[PROPERTY_VALUE_MAX] = {};

    int len = property_get(key, buf, "");
    if (len == 1) {
        char ch = buf[0];
        if (ch == '0' || ch == 'n') {
            result = false;
        } else if (ch == '1' || ch == 'y') {
            result = true;
        }
    } else if (len > 1) {
        if (!strcmp(buf, "no") || !strcmp(buf, "false") || !strcmp(buf, "off")) {
            result = false;
        } else if (!strcmp(buf, "yes") || !strcmp(buf, "true") || !strcmp(buf, "on")) {
            result = true;
        }
    }

    return result;
}

#ifdef __cplusplus
}
#endif

template <typename T>
static T property_get_int(const char* key, T default_value) {
    if (!key) return default_value;

    char value[PROPERTY_VALUE_MAX] = {};
    if (property_get(key, value, "") < 1) return default_value;

    // libcutils unwisely allows octal, which libbase doesn't.
    T result = default_value;
    int saved_errno = errno;
    errno = 0;
    char* end = nullptr;
    intmax_t v = strtoimax(value, &end, 0);
    if (errno != ERANGE && end != value && v >= std::numeric_limits<T>::min() &&
        v <= std::numeric_limits<T>::max()) {
        result = v;
    }
    errno = saved_errno;
    return result;
}

#ifdef __cplusplus
extern "C" {
#endif

int64_t property_get_int64(const char* key, int64_t default_value) {
    return property_get_int<int64_t>(key, default_value);
}

int32_t property_get_int32(const char* key, int32_t default_value) {
    return property_get_int<int32_t>(key, default_value);
}

int property_set(const char* key, const char* value) {
    return g_set_callback(key, value);
}

int property_get(const char* key, char* value, const char* default_value) {
    int len = g_get_callback(key, value);
    if (len < 1 ) {
        len = property_internal::__internal_property_get(key, value);
        if( len < 1 && default_value ) {
            snprintf(value, PROPERTY_VALUE_MAX, "%s", default_value);
        }
        return strlen(value);
    }
    return len;
}

#if __has_include(<sys/system_properties.h>)

#include <sys/system_properties.h>

#endif

struct callback_data {
    void (*callback)(const char* name, const char* value, void* cookie);
    void* cookie;
};

static void trampoline(void* raw_data, const char* name, const char* value, unsigned /*serial*/) {
    callback_data* data = reinterpret_cast<callback_data*>(raw_data);
    data->callback(name, value, data->cookie);
}

#ifdef _MSC_VER

int property_list( void ( *fn )( const char* name, const char* value, void* cookie ), void* cookie )
{
    property_internal::foreach_fun fun = std::bind( fn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 );
    property_internal::foreach_properties( fun, cookie );
    return 0;
}

#else

static void property_list_callback(const prop_info* pi, void* data) {
    __system_property_read_callback(pi, trampoline, data);
}

int property_list(void (*fn)(const char* name, const char* value, void* cookie), void* cookie) {
    callback_data data = {fn, cookie};
    return __system_property_foreach(property_list_callback, &data);
}
#endif

#ifdef __cplusplus
}
#endif
