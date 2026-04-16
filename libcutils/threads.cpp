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

#include <algorithm>
#include <atomic>
#include <chrono>
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

int pthread_setname_np(pthread_t __pthread, const char* __name)
{
    auto current_id = pthread_manager::get_instance().try_to_get_current_id();
    if( ( current_id == __pthread ) ||
        ( 0 == __pthread ) )
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

    uint64_t underly_id = gettid();
    if (__pthread == underly_id)
    {
        base::PlatformThread::SetName(__name);
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

int pthread_mutexattr_init( pthread_mutexattr_t* a_mutext_attr )
{
    return 0;
}

int pthread_mutexattr_setprotocol( pthread_mutexattr_t* a_mutex_attr, int a_protocol )
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

int pthread_mutex_timedlock( pthread_mutex_t* const mutex, struct timespec const* a_time )
{
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

#define MS_PER_SEC      1000ULL     // MS = milliseconds
#define US_PER_MS       1000ULL     // US = microseconds
#define HNS_PER_US      10ULL       // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US       1000ULL

#define HNS_PER_SEC     (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS      (100ULL)    // NS = nanoseconds
#define NS_PER_SEC      (MS_PER_SEC * US_PER_MS * NS_PER_US)

static int __clock_gettime_realtime( struct timespec* tv )
{
    FILETIME ft;
    ULARGE_INTEGER hnsTime;

    GetSystemTimePreciseAsFileTime( &ft );

    hnsTime.LowPart = ft.dwLowDateTime;
    hnsTime.HighPart = ft.dwHighDateTime;

    // To get POSIX Epoch as baseline, subtract the number of hns intervals from Jan 1, 1601 to Jan 1, 1970.
    hnsTime.QuadPart -= ( 11644473600ULL * HNS_PER_SEC );

    // modulus by hns intervals per second first, then convert to ns, as not to lose resolution
    tv->tv_nsec = (long)( ( hnsTime.QuadPart % HNS_PER_SEC ) * NS_PER_HNS );
    tv->tv_sec = (long)( hnsTime.QuadPart / HNS_PER_SEC );

    return 0;
}

int pthread_cond_timedwait( pthread_cond_t* const cond,
                            pthread_mutex_t* const mutex, struct timespec* time )
{
    if( !time )
    {
        return pthread_cond_wait( cond, mutex );
    }

    struct timespec now;
    __clock_gettime_realtime( &now );

    std::chrono::nanoseconds request_end_time = std::chrono::seconds( time->tv_sec );
    request_end_time += std::chrono::nanoseconds( time->tv_nsec );

    std::chrono::nanoseconds request_now_time = std::chrono::seconds( now.tv_sec );
    request_now_time += std::chrono::nanoseconds( now.tv_nsec );

    std::chrono::nanoseconds duration_nanos = request_end_time - request_now_time;
    if( duration_nanos.count() <= 0 )
    {
        return pthread_cond_wait( cond, mutex );
    }

    uint64_t dur_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>( duration_nanos ).count();
    return !SleepConditionVariableSRW( cond, mutex, dur_milliseconds, 0 );
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

int getpriority( int which, int who )
{
    constexpr int CLASS_WEIGHT[] =
    {
        0,  // IDLE_PRIORITY_CLASS
        1,  // BELOW_NORMAL_PRIORITY_CLASS
        2,  // NORMAL_PRIORITY_CLASS
        3,  // ABOVE_NORMAL_PRIORITY_CLASS
        4,  // HIGH_PRIORITY_CLASS
        5   // REALTIME_PRIORITY_CLASS
    };

    // 线程相对优先级权重：-2…2 映射到 0…4
    constexpr int REL_WEIGHT[] =
    {
        0,  // THREAD_PRIORITY_IDLE         (-15)
        1,  // THREAD_PRIORITY_LOWEST       (-2)
        2,  // THREAD_PRIORITY_BELOW_NORMAL (-1)
        3,  // THREAD_PRIORITY_NORMAL       (0)
        4   // THREAD_PRIORITY_ABOVE_NORMAL (+1)
        // 其余 HIGHEST/TIME_CRITICAL 也按 4 处理，简化
    };

    if (who != 0)
    {
        errno = EINVAL;
        return -21;
    }

    DWORD pc = 0;
    int   tc = 0;

    switch (which)
    {
    case PRIO_PROCESS:
    case PRIO_PGRP:
        pc = GetPriorityClass(GetCurrentProcess());
        tc = THREAD_PRIORITY_NORMAL; // 取主线程缺省相对优先级
        break;
    case PRIO_USER: // 把“用户”当成当前线程
        pc = GetPriorityClass(GetCurrentProcess());
        tc = GetThreadPriority(GetCurrentThread());
        break;
    default:
        errno = EINVAL;
        return -21;
    }

    // 把 pc 转成下标
    int cidx = 2; // 默认 NORMAL
    if (pc == IDLE_PRIORITY_CLASS)          cidx = 0;
    else if (pc == BELOW_NORMAL_PRIORITY_CLASS) cidx = 1;
    else if (pc == ABOVE_NORMAL_PRIORITY_CLASS) cidx = 3;
    else if (pc == HIGH_PRIORITY_CLASS)         cidx = 4;
    else if (pc == REALTIME_PRIORITY_CLASS)     cidx = 5;

    // 把 tc 粗粒度化
    int tidx = 3;
    if (tc <= THREAD_PRIORITY_IDLE)         tidx = 0;
    else if (tc <= THREAD_PRIORITY_LOWEST)      tidx = 1;
    else if (tc <= THREAD_PRIORITY_BELOW_NORMAL)tidx = 2;
    else if (tc >= THREAD_PRIORITY_ABOVE_NORMAL)tidx = 4;

    // 线性映射到 -20…19
    int w = CLASS_WEIGHT[cidx] * 5 + REL_WEIGHT[tidx];
    return std::clamp(-20 + w, -20, 19);
}

int setpriority( int which, int who, int nice )
{
    if( which != PRIO_PROCESS || who != 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( nice < -20 || nice > 19 )
    {
        errno = EINVAL;
        return -1;
    }

    /* 把 -20…19 的 40 段映射到 0…29 的权重区间 */
    int w = nice + 20;          // 0…39
    int classIdx = w / 7;       // 0…5
    int relIdx = ( w % 7 ) / 2; // 0…4 粗粒度化

    /* 边界保护 */
    if( classIdx > 5 ) classIdx = 5;
    if( relIdx > 4 ) relIdx = 4;

    /* 权重 → Windows 常量 */
    DWORD priorityClass = NORMAL_PRIORITY_CLASS;
    switch( classIdx )
    {
    case 0: priorityClass = IDLE_PRIORITY_CLASS;          break;
    case 1: priorityClass = BELOW_NORMAL_PRIORITY_CLASS;  break;
    case 2: priorityClass = NORMAL_PRIORITY_CLASS;        break;
    case 3: priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;  break;
    case 4: priorityClass = HIGH_PRIORITY_CLASS;          break;
    case 5: priorityClass = REALTIME_PRIORITY_CLASS;      break;
    }

    int threadPrio = THREAD_PRIORITY_NORMAL;
    switch( relIdx )
    {
    case 0: threadPrio = THREAD_PRIORITY_IDLE;         break;
    case 1: threadPrio = THREAD_PRIORITY_LOWEST;       break;
    case 2: threadPrio = THREAD_PRIORITY_BELOW_NORMAL; break;
    case 3: threadPrio = THREAD_PRIORITY_NORMAL;       break;
    case 4: threadPrio = THREAD_PRIORITY_ABOVE_NORMAL; break;
    }

    /* 先设进程优先级类 */
    if( !SetPriorityClass( GetCurrentProcess(), priorityClass ) )
    {
        errno = EPERM;
        return -1;
    }

    /* 再设主线程相对优先级（调试够用） */
    if( !SetThreadPriority( GetCurrentThread(), threadPrio ) )
    {
        errno = EPERM;
        return -1;
    }

    return 0;
}

#define SCHED_OTHER  0
#define SCHED_BATCH  3
#define SCHED_IDLE   5

int sched_getscheduler( pid_t pid )
{
    if( pid != 0 )
    {
        errno = EINVAL;
        return -1;
    }

    DWORD pc = GetPriorityClass( GetCurrentProcess() );
    int   tc = GetThreadPriority( GetCurrentThread() );

    /* 1. 最容易识别的极端值 */
    if( pc == IDLE_PRIORITY_CLASS )
    {
        return ( tc <= THREAD_PRIORITY_IDLE ) ? SCHED_IDLE : SCHED_BATCH;
    }

    if( pc == REALTIME_PRIORITY_CLASS )
    {
        /* 实时类里再分 RR / FIFO：简单按线程优先级阈值划分 */
        return ( tc >= THREAD_PRIORITY_TIME_CRITICAL ) ? SCHED_FIFO : SCHED_RR;
    }

    /* 2. 其余全部算 SCHED_OTHER */
    return SCHED_OTHER;
}

}

#endif /* !defined(_WIN32) */

#endif
