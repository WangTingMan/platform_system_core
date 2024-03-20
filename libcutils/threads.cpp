/*
** Copyright (C) 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <cutils/threads.h>

#if defined(__APPLE__)
#include <stdint.h>
#elif defined(__linux__)
#include <syscall.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <thread>

#if defined(__BIONIC__) || defined(__GLIBC__) && __GLIBC_MINOR__ >= 32
// No definition needed for Android because we'll just pick up bionic's copy.
// No definition needed for Glibc >= 2.32 because it exposes its own copy.
#else
pid_t gettid() {
#if defined(__APPLE__)
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  return tid;
#elif defined(__linux__)
  return syscall(__NR_gettid);
#elif defined(_WIN32)
  return GetCurrentThreadId();
#endif
}


#if !defined(_WIN32)

void* thread_store_get( thread_store_t* store )
{
    if( !store->has_tls )
        return NULL;

    return pthread_getspecific( store->tls );
}

extern void   thread_store_set( thread_store_t* store,
    void* value,
    thread_store_destruct_t  destroy )
{
    pthread_mutex_lock( &store->lock );
    if( !store->has_tls )
    {
        if( pthread_key_create( &store->tls, destroy ) != 0 )
        {
            pthread_mutex_unlock( &store->lock );
            return;
        }
        store->has_tls = 1;
    }
    pthread_mutex_unlock( &store->lock );

    pthread_setspecific( store->tls, value );
}

#else /* !defined(_WIN32) */
void* thread_store_get( thread_store_t* store )
{
    if( !store->has_tls )
        return NULL;

    return ( void* )TlsGetValue( store->tls );
}

void   thread_store_set( thread_store_t* store,
    void* value,
    thread_store_destruct_t  /*destroy*/ )
{
    /* XXX: can't use destructor on thread exit */
    if( !store->lock_init )
    {
        store->lock_init = -1;
        InitializeCriticalSection( &store->lock );
        store->lock_init = -2;
    }
    else while( store->lock_init != -2 )
    {
        Sleep( 10 ); /* 10ms */
    }

    EnterCriticalSection( &store->lock );
    if( !store->has_tls )
    {
        store->tls = TlsAlloc();
        if( store->tls == TLS_OUT_OF_INDEXES )
        {
            LeaveCriticalSection( &store->lock );
            return;
        }
        store->has_tls = 1;
    }
    LeaveCriticalSection( &store->lock );

    TlsSetValue( store->tls, value );
}

extern "C" void usleep( uint32_t time_ )
{
    std::this_thread::sleep_for( std::chrono::microseconds( time_ ) );
}

extern "C" void sleep( uint32_t time_ )
{
    std::this_thread::sleep_for( std::chrono::seconds( time_ ) );
}

extern "C" int nanosleep( const struct timespec* req, struct timespec* rem )
{
    auto start_ = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::nanoseconds( req->tv_nsec ) + std::chrono::seconds( req->tv_sec );
    std::this_thread::sleep_for( duration );
    auto end_ = std::chrono::high_resolution_clock::now();
    return 0;
}

#endif /* !defined(_WIN32) */

#endif
