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

#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include <utils/Compat.h>
#include <utils/utils_export.h>

#ifdef __cplusplus
#include <functional>
#include <string>
#endif

#ifdef _WINSOCKAPI_
#define DO_NOT_DEFINE_TIME_VAL
#endif

#ifndef WINSOCK_API_LINKAGE
 /*
  * Structure used in select() call, taken from the BSD file sys/time.h.
  */
#ifndef DO_NOT_DEFINE_TIME_VAL
struct timeval {
    long    tv_sec;         /* seconds */
    long    tv_usec;        /* and microseconds */
};
#endif
#endif

// ------------------------------------------------------------------
// C API

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t nsecs_t;       // nano-seconds

struct timezone
{
    int tz_minuteswest; /* of Greenwich */
    int tz_dsttime;     /* type of dst correction to apply */
};

static CONSTEXPR inline nsecs_t seconds_to_nanoseconds(nsecs_t secs)
{
    return secs*1000000000;
}

static CONSTEXPR inline nsecs_t milliseconds_to_nanoseconds(nsecs_t secs)
{
    return secs*1000000;
}

static CONSTEXPR inline nsecs_t microseconds_to_nanoseconds(nsecs_t secs)
{
    return secs*1000;
}

static CONSTEXPR inline nsecs_t nanoseconds_to_seconds(nsecs_t secs)
{
    return secs/1000000000;
}

static CONSTEXPR inline nsecs_t nanoseconds_to_milliseconds(nsecs_t secs)
{
    return secs/1000000;
}

static CONSTEXPR inline nsecs_t nanoseconds_to_microseconds(nsecs_t secs)
{
    return secs/1000;
}

static CONSTEXPR inline nsecs_t s2ns(nsecs_t v)  {return seconds_to_nanoseconds(v);}
static CONSTEXPR inline nsecs_t ms2ns(nsecs_t v) {return milliseconds_to_nanoseconds(v);}
static CONSTEXPR inline nsecs_t us2ns(nsecs_t v) {return microseconds_to_nanoseconds(v);}
static CONSTEXPR inline nsecs_t ns2s(nsecs_t v)  {return nanoseconds_to_seconds(v);}
static CONSTEXPR inline nsecs_t ns2ms(nsecs_t v) {return nanoseconds_to_milliseconds(v);}
static CONSTEXPR inline nsecs_t ns2us(nsecs_t v) {return nanoseconds_to_microseconds(v);}

static CONSTEXPR inline nsecs_t seconds(nsecs_t v)      { return s2ns(v); }
static CONSTEXPR inline nsecs_t milliseconds(nsecs_t v) { return ms2ns(v); }
static CONSTEXPR inline nsecs_t microseconds(nsecs_t v) { return us2ns(v); }

enum {
    SYSTEM_TIME_REALTIME = 0,   // system-wide realtime clock
    SYSTEM_TIME_MONOTONIC = 1,  // monotonic time since unspecified starting point
    SYSTEM_TIME_PROCESS = 2,    // high-resolution per-process clock
    SYSTEM_TIME_THREAD = 3,     // high-resolution per-thread clock
    SYSTEM_TIME_BOOTTIME = 4,   // same as SYSTEM_TIME_MONOTONIC, but including CPU suspend time
};

// return the system-time according to the specified clock
#ifdef __cplusplus
UTILS_EXPORT nsecs_t systemTime(int clock = SYSTEM_TIME_MONOTONIC);
#else
nsecs_t systemTime(int clock);
#endif // def __cplusplus

/**
 * Returns the number of milliseconds to wait between the reference time and the timeout time.
 * If the timeout is in the past relative to the reference time, returns 0.
 * If the timeout is more than INT_MAX milliseconds in the future relative to the reference time,
 * such as when timeoutTime == LLONG_MAX, returns -1 to indicate an infinite timeout delay.
 * Otherwise, returns the difference between the reference time and timeout time
 * rounded up to the next millisecond.
 */
UTILS_EXPORT int toMillisecondTimeoutDelay(nsecs_t referenceTime, nsecs_t timeoutTime);

UTILS_EXPORT int gettimeofday(struct timeval* p, struct timezone* z);

UTILS_EXPORT void localtime_r(const time_t *secs, struct tm *time);

#if defined(_MSC_VER)

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 2
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4 
#endif

#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME 7
#endif

#ifndef clockid_t
#define clockid_t int
#endif

UTILS_EXPORT int clock_gettime(int type, struct timespec* time);
#endif

typedef void (*timer_expired_callback_type)(void* para);
typedef void (*timer_expired_callback_no_paras_type)();

UTILS_EXPORT uint32_t common_timer_create(timer_expired_callback_type a_callback, void* a_user_data);

UTILS_EXPORT uint32_t common_timer_create_no_paras(timer_expired_callback_no_paras_type a_callback);

UTILS_EXPORT void stop_timer( uint32_t timer_id);

UTILS_EXPORT void delete_timer( uint32_t timer_id);

UTILS_EXPORT void set_timer_duration( uint32_t timer_id, int milliseconds);

UTILS_EXPORT void set_timer( uint32_t timer_id, int milliseconds,
    timer_expired_callback_type a_callback, void* a_user_data );

/**
 * Return 0 if not scheduled, otherwise return not 0( maybe 1 )
 */
UTILS_EXPORT int is_timer_scheduled( uint32_t timer_id );

UTILS_EXPORT int get_timer_remaining_ms( uint32_t timer_id );

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
UTILS_EXPORT uint32_t create_timer( std::function<void()> a_fun, std::string a_name = "", bool a_periodic = true );
UTILS_EXPORT void set_timer_option( uint32_t timer_id, int milliseconds, std::function<void()> a_fun );
#endif
