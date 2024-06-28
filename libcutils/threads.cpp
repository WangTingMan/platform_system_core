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
#define NOMINMAX

#include <cutils/threads.h>

#if defined(__APPLE__)
#include <stdint.h>
#elif defined(__linux__)
#include <syscall.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <vector>

#include <base/threading/platform_thread.h>
#include <base/strings/sys_string_conversions.h>

#define COLD

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

extern "C" {

struct pthread_cb
{
    int id;
    uint64_t underlying_id = 0;
    std::shared_ptr<std::thread> thread;
    std::string thread_name;
};

class pthread_manager
{

public:

    static pthread_manager& get_instance();

    pthread_cb* find_thread( int id );

    int add_thread( pthread_cb cb );

    void remove_thread( int id );

    int get_next_id();

    void set_underlying_id
        (
        int id,
        uint64_t underlying_id
        )
    {
        std::lock_guard lker( m_mutex );
        for (auto& ele : m_threads)
        {
            if (ele.id == id)
            {
                ele.underlying_id = underlying_id;
                return;
            }
        }
    }

    uint64_t get_underlying_id(int id)
    {
        std::lock_guard lker(m_mutex);
        for (auto& ele : m_threads)
        {
            if (ele.id == id)
            {
                return ele.underlying_id;
            }
        }
        return 0;
    }

    int try_to_get_current_id()
    {
        uint64_t underly_id = gettid();
        std::lock_guard lker(m_mutex);
        for (auto& ele : m_threads)
        {
            if (ele.underlying_id == underly_id)
            {
                return ele.id;
            }
        }
        return 0;
    }

    bool try_to_set_name_delay(int id, std::string name)
    {
        std::lock_guard lker(m_mutex);
        for (auto& ele : m_threads)
        {
            if (ele.id == id)
            {
                ele.thread_name = name;
                return true;
            }
        }
        return false;
    }

    std::string try_to_get_reset_name(int id)
    {
        std::lock_guard lker(m_mutex);
        for (auto& ele : m_threads)
        {
            if (ele.id == id)
            {
                return ele.thread_name;
            }
        }
        return "";
    }

private:

    std::atomic_int32_t m_current_id = 10;
    std::mutex m_mutex;
    std::vector<pthread_cb> m_threads;
};

pthread_manager& pthread_manager::get_instance()
{
    static pthread_manager instance;
    return instance;
}

pthread_cb* pthread_manager::find_thread( int id )
{
    std::lock_guard<std::mutex> lcker( m_mutex );
    for( int i = 0; i < m_threads.size(); ++i )
    {
        if( id == m_threads[i].id )
        {
            return &m_threads[i];
        }
    }
    return nullptr;
}

int pthread_manager::add_thread( pthread_cb cb )
{
    std::lock_guard<std::mutex> lcker( m_mutex );
    m_threads.emplace_back( cb );
    return cb.id;
}

void pthread_manager::remove_thread( int id )
{
    std::lock_guard<std::mutex> lcker( m_mutex );
    for( auto it = m_threads.begin(); it != m_threads.end(); ++it )
    {
        if( it->id == id )
        {
            m_threads.erase( it );
            return;
        }
    }
}

int pthread_manager::get_next_id()
{
    return m_current_id++;
}

void thread_running_task
    (
    std::function<void()> a_detail_task,
    int a_init_id
    )
{
    auto preset_name = pthread_manager::get_instance()
        .try_to_get_reset_name(a_init_id);
    if (!preset_name.empty())
    {
        base::PlatformThread::SetName(preset_name);
    }

    uint64_t underly_id = GetCurrentThreadId();
    pthread_manager::get_instance().set_underlying_id(a_init_id, underly_id);
    a_detail_task();
}

COLD void dav1d_init_thread( void )
{
}

COLD int dav1d_pthread_create( pthread_t* const thread,
                               const pthread_attr_t* const attr,
                               void* ( * const func )( void* ), void* const arg )
{
    std::function<void()> fun = std::bind(func, arg);
    pthread_cb cb;
    cb.id = pthread_manager::get_instance().get_next_id();
    std::shared_ptr<std::thread> thread_ = std::make_shared<std::thread>
        (thread_running_task, fun, cb.id);
    cb.thread = thread_;
    *thread = cb.id;
    pthread_manager::get_instance().add_thread(cb);
    return 0;
}

COLD int dav1d_pthread_join( pthread_t* thread, void** res )
{
    pthread_cb* cb = pthread_manager::get_instance().find_thread( *thread );
    if( cb && cb->thread )
    {
        if( cb->thread->joinable() )
        {
            cb->thread->join();
        }
    }

    return 0;
}

COLD int dav1d_pthread_once( pthread_once_t* const once_control,
                             void ( * const init_routine )( void ) )
{
    BOOL pending = FALSE;

    if( InitOnceBeginInitialize( once_control, 0, &pending, NULL ) != TRUE )
        return 1;

    if( pending == TRUE )
        init_routine();

    return !InitOnceComplete( once_control, 0, NULL );
}

int pthread_attr_init( pthread_attr_t* const attr )
{
    attr->stack_size = 0;
    return 0;
}

int pthread_attr_destroy( pthread_attr_t* const attr )
{
    return 0;
}

int pthread_attr_setstacksize( pthread_attr_t* const attr,
                                             const size_t stack_size )
{
    if( stack_size > UINT_MAX ) return 1;
    attr->stack_size = (unsigned)stack_size;
    return 0;
}

int pthread_attr_setdetachstate( pthread_attr_t* a, int state )
{
    return 0;
}

int pthread_setname_np( pthread_t __pthread, const char* __name )
{
    auto current_id = pthread_manager::get_instance().try_to_get_current_id();
    if (current_id == __pthread)
    {
        base::PlatformThread::SetName(__name);
        return 0;
    }

    uint64_t underlying_id = pthread_manager::get_instance().get_underlying_id(__pthread);
    if (underlying_id > 0)
    {
        HANDLE hdl = OpenThread(THREAD_ALL_ACCESS, FALSE, underlying_id);
        if (NULL != hdl)
        {
            std::wstring thread_name = base::SysNativeMBToWide(__name);
            SetThreadDescription(hdl, thread_name.c_str());
            CloseHandle(hdl);
            return 0;
        }
    }

    bool preset = pthread_manager::get_instance().try_to_set_name_delay(__pthread, __name);
    if (preset)
    {
        return 0;
    }

    LOG(ERROR) << "This thread is not managed by pthread manager. Cannot set name now";
    return -1;
}

int pthread_getschedparam( pthread_t t, int* policy, struct sched_param* param )
{
    return 0;
}

int pthread_setschedparam( pthread_t t, int policy, const struct sched_param* param )
{
    return 0;
}

int sched_get_priority_min( int policy )
{
    return 0;
}

int sched_get_priority_max( int policy )
{
    return 100;
}

int pthread_self()
{
    auto current_id = pthread_manager::get_instance().try_to_get_current_id();
    if (current_id > 0)
    {
        return current_id;
    }

    return gettid();
}

int sched_setscheduler( pid_t __pid, int __policy, const struct sched_param* __param )
{
    return 0;
}

int pthread_mutex_init( pthread_mutex_t* const mutex,
                                      const void* const attr )
{
    InitializeSRWLock( mutex );
    return 0;
}

int pthread_mutex_destroy( pthread_mutex_t* const mutex )
{
    return 0;
}

int pthread_mutex_lock( pthread_mutex_t* const mutex )
{
    AcquireSRWLockExclusive( mutex );
    return 0;
}

int pthread_mutex_unlock( pthread_mutex_t* const mutex )
{
    ReleaseSRWLockExclusive( mutex );
    return 0;
}

int pthread_cond_init( pthread_cond_t* const cond,
                                     const void* const attr )
{
    InitializeConditionVariable( cond );
    return 0;
}

int pthread_cond_destroy( pthread_cond_t* const cond )
{
    return 0;
}

int pthread_cond_wait( pthread_cond_t* const cond,
                                     pthread_mutex_t* const mutex )
{
    return !SleepConditionVariableSRW( cond, mutex, INFINITE, 0 );
}

int pthread_cond_signal( pthread_cond_t* const cond )
{
    WakeConditionVariable( cond );
    return 0;
}

int pthread_cond_broadcast( pthread_cond_t* const cond )
{
    WakeAllConditionVariable( cond );
    return 0;
}

}

#endif /* !defined(_WIN32) */

#endif
