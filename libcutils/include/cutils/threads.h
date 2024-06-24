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

#include <mutex>
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

#if defined(_WIN32)

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_CREATE_JOINABLE 0

#define PTHREAD_EXPLICIT_SCHED 0
#define PTHREAD_INHERIT_SCHED 1

#define PTHREAD_PRIO_NONE 0
#define PTHREAD_PRIO_INHERIT 1

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1

#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1

typedef int pthread_t;

typedef struct
{
    unsigned stack_size;
} pthread_attr_t;

#define SCHED_NORMAL 0
#define SCHED_OTHER SCHED_NORMAL

/**
 * See sched_getparam()/sched_setparam() and
 * sched_getscheduler()/sched_setscheduler().
 */
struct sched_param
{
    int sched_priority;
};

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef INIT_ONCE pthread_once_t;

CUTILS_EXPORT int dav1d_pthread_create( pthread_t* thread, const pthread_attr_t* attr,
                          void* ( *func )( void* ), void* arg );
CUTILS_EXPORT int dav1d_pthread_join( pthread_t* thread, void** res );
CUTILS_EXPORT int dav1d_pthread_once( pthread_once_t* once_control,
                        void ( *init_routine )( void ) );

#define pthread_create dav1d_pthread_create
#define pthread_join(thread, res) dav1d_pthread_join(&(thread), res)
#define pthread_once   dav1d_pthread_once

CUTILS_EXPORT int pthread_attr_init( pthread_attr_t* const attr );

CUTILS_EXPORT int pthread_attr_destroy( pthread_attr_t* const attr );

CUTILS_EXPORT int pthread_attr_setstacksize( pthread_attr_t* const attr,
                                             const size_t stack_size );

CUTILS_EXPORT int pthread_attr_setdetachstate( pthread_attr_t* a, int state );

CUTILS_EXPORT int pthread_setname_np( pthread_t __pthread, const char* __name );

CUTILS_EXPORT int pthread_getschedparam( pthread_t t, int* policy, struct sched_param* param );

CUTILS_EXPORT int pthread_setschedparam( pthread_t t, int policy, const struct sched_param* param );

CUTILS_EXPORT int sched_get_priority_min( int policy );

CUTILS_EXPORT int sched_get_priority_max( int policy );

CUTILS_EXPORT int pthread_self();

#ifndef SCHED_FIFO
#define SCHED_FIFO 1
#endif
CUTILS_EXPORT int sched_setscheduler( pid_t __pid, int __policy, const struct sched_param* __param );

CUTILS_EXPORT int pthread_mutex_init( pthread_mutex_t* const mutex,
                                      const void* const attr );

CUTILS_EXPORT int pthread_mutex_destroy( pthread_mutex_t* const mutex );

CUTILS_EXPORT int pthread_mutex_lock( pthread_mutex_t* const mutex );

CUTILS_EXPORT int pthread_mutex_unlock( pthread_mutex_t* const mutex );

CUTILS_EXPORT int pthread_cond_init( pthread_cond_t* const cond,
                                     const void* const attr );

CUTILS_EXPORT int pthread_cond_destroy( pthread_cond_t* const cond );

CUTILS_EXPORT int pthread_cond_wait( pthread_cond_t* const cond,
                                     pthread_mutex_t* const mutex );

CUTILS_EXPORT int pthread_cond_signal( pthread_cond_t* const cond );

CUTILS_EXPORT int pthread_cond_broadcast( pthread_cond_t* const cond );

#endif

#ifdef __cplusplus
}
#endif
