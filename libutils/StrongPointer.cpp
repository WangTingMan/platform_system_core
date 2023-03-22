/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "sp"
#define NOMINMAX

#include <utils/StrongPointer.h>

#include <log/log.h>
#include <type_traits>
#include <utility>
#include <algorithm>

#ifdef _MSC_VER
#include <windows.h>
#endif

namespace android {

void sp_report_race() { LOG_ALWAYS_FATAL("sp<> assignment detected data race"); }

void* retrieve_frame_address( int level )
{
#ifdef _MSC_VER
    int count_ = 0;
    constexpr int kMaxTraces = 62;
    void* trace_[kMaxTraces + 1];
    int get_count = std::min( kMaxTraces, level );
    count_ = CaptureStackBackTrace( 0, kMaxTraces, trace_, NULL );
    int index = std::min( kMaxTraces, std::min( count_, level ) );
    return ( trace_[index] );
#else
    return ( __builtin_frame_address( level ) );
#endif
    return 0;
}

}
