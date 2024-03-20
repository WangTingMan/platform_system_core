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

#pragma once

#include <cutils\cutils_export.h>

#include  <sys/types.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef pid_t
#define pid_t int
#endif

//
// Deprecated: use android::base::GetThreadId instead, which doesn't truncate on Mac/Windows.
//
#if !defined(__GLIBC__) || __GLIBC__ >= 2 && __GLIBC_MINOR__ < 32
CUTILS_EXPORT pid_t gettid();
#endif

#if !defined(_WIN32)

typedef struct
{
    pthread_mutex_t   lock;
    int               has_tls;
    pthread_key_t     tls;
} thread_store_t;

#define  THREAD_STORE_INITIALIZER  { PTHREAD_MUTEX_INITIALIZER, 0, 0 }

#else // !defined(_WIN32)

typedef struct
{
    int               lock_init;
    int               has_tls;
    DWORD             tls;
    CRITICAL_SECTION  lock;
} thread_store_t;

#define  THREAD_STORE_INITIALIZER  { 0, 0, 0, {0, 0, 0, 0, 0, 0} }

#endif // !defined(_WIN32)

typedef void  ( *thread_store_destruct_t )( void* value );

CUTILS_EXPORT void* thread_store_get( thread_store_t* store );

CUTILS_EXPORT void thread_store_set( thread_store_t* store,
    void* value,
    thread_store_destruct_t  destroy );

CUTILS_EXPORT void usleep( uint32_t time_ );

CUTILS_EXPORT void sleep( uint32_t time_ );

CUTILS_EXPORT int nanosleep( const struct timespec* req, struct timespec* rem );

#ifdef __cplusplus
}
#endif
