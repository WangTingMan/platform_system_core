#include "message_loop_thread.h"

#include <cmsis_os.h>
#include <cutils/threads.h>

#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <thread>

class thread_signaling
{

public:

    thread_signaling();

    int32_t set_value( int32_t a_signaling_value );

    int32_t get_value()
    {
        std::lock_guard locker( m_mutex );
        return m_signaling_value;
    }

    osEvent wait_value( int32_t a_signaling_value, uint64_t a_millisecs );

    int32_t clear_value( int32_t a_signaling_value );

private:

    bool check_signal_match( int32_t a_value );

    uint32_t m_signaling_value = 0;
    std::mutex m_mutex;
    std::condition_variable m_condtion;
};

struct os_semaphore_cb
{
    std::counting_semaphore<0xFFFFFF> m_semaphore_detail;
    std::atomic<int32_t> m_count;
};

struct os_pool_cb
{
    uint32_t item_size;
};

struct os_messageQ_cb
{
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::queue<uint32_t> m_queue;
    uint32_t m_max_count;
};

struct os_mailQ_cb
{
    std::mutex m_mtx;
    std::condition_variable m_cv_put;  // 等待有空位可以put
    std::condition_variable m_cv_get;  // 等待有mail可以get
    std::queue<void*> m_mail_queue;
    uint32_t m_max_mail;
    uint32_t m_item_size;
    uint32_t m_used_count; // 当前已经alloc出去、还没free的块总数
};

#ifdef __cplusplus
extern "C" {
#endif

struct os_thread_cb
{
    uint64_t thread_id;
    uint16_t has_onwership;
    osPriority m_priority = osPriorityNormal;
    thread_signaling m_thread_signaling;
};

struct os_timer_cb
{
    int64_t id;
};

struct os_mutex_cb
{
    std::recursive_timed_mutex m_mutex;
};

void free_resource( void* );
struct os_thread_cb* allocate_one_k_thread();

#ifdef __cplusplus
}
#endif

osThreadId s_background_timer_thread;

#ifdef __cplusplus
extern "C" {
#endif

struct thread_parameters_wrapper
{
    osThreadDef_t thread_def;
    void* argument = nullptr;
    bool run_and_quit = false;
    uint64_t id = 0;
};

void* os_pthread_entry( thread_parameters_wrapper argument )
{
    thread_parameters_wrapper* wrapper = &argument;

    if( wrapper->thread_def.pthread )
    {
        wrapper->thread_def.pthread( wrapper->argument );
    }

    if( argument.run_and_quit )
    {
        auto thread = message_loop_thread_manager::get_instance()
            .get_message_loop( wrapper->id );
        if( thread )
        {
            thread->ShutDown();
        }
    }
    return nullptr;
}

osStatus osKernelInitialize( void )
{
    return osOK;
}

osStatus osKernelStart( void )
{
    std::shared_ptr<os_thread_cb> _thread;
    _thread.reset( allocate_one_k_thread(), free_resource );

    uint64_t id = 0;
    id = message_loop_thread_manager::get_instance().create_new_message_loop( "" );
    _thread->thread_id = id;

    message_loop_thread_manager::get_instance().keep_thread_cb( _thread );

    auto thread = message_loop_thread_manager::get_instance().get_message_loop( id );

    thread_parameters_wrapper wrapper;
    wrapper.argument = nullptr;
    wrapper.thread_def.pthread = nullptr;
    wrapper.run_and_quit = false;
    wrapper.id = id;
    std::function<void()> fun = std::bind( os_pthread_entry, wrapper );
    thread->DoInThread( []()
        {
            base::PlatformThread::SetName( "background_timer_thread" );
        } );

    return osOK;
}

uint32_t osKernelSysTick( void )
{
    return osOK;
}

int32_t osKernelRunning( void )
{
    return 1;
}

osThreadId osThreadCreate( const osThreadDef_t* thread_def, void* argument )
{
    std::shared_ptr<os_thread_cb> _thread;
    _thread.reset( allocate_one_k_thread(), free_resource );

    uint64_t id = 0;
    id = message_loop_thread_manager::get_instance().create_new_message_loop( "" );
    _thread->thread_id = id;

    message_loop_thread_manager::get_instance().keep_thread_cb( _thread );

    auto thread = message_loop_thread_manager::get_instance().get_message_loop( id );

    thread_parameters_wrapper wrapper;
    wrapper.argument = argument;
    wrapper.thread_def = *thread_def;
    wrapper.run_and_quit = true;
    wrapper.id = id;
    std::function<void()> fun = std::bind( os_pthread_entry, wrapper );
    thread->DoInThread( fun );

    return _thread.get();
}

osThreadId osThreadGetId( void )
{
    uint64_t id = base::PlatformThread::CurrentId();
    auto cb = message_loop_thread_manager::get_instance().get_thread_cb( id );
    return cb.get();
}

osStatus osThreadTerminate( osThreadId thread_id )
{
    uint64_t id = thread_id->thread_id;
    auto thread = message_loop_thread_manager::get_instance().get_message_loop( id );
    thread->ShutDown();
    return osOK;
}

osStatus osThreadYield( void )
{
    std::this_thread::yield();
    return osOK;
}

osStatus osThreadSetPriority( osThreadId thread_id, osPriority priority )
{
    thread_id->m_priority = priority;
    return osOK;
}

osPriority osThreadGetPriority( osThreadId thread_id )
{
    return thread_id->m_priority;
}

osStatus osDelay( uint32_t millisec )
{
    std::this_thread::sleep_for( std::chrono::milliseconds( millisec ) );
    return osOK;
}

osEvent osWait( uint32_t millisec )
{
    return osEvent();
}

osTimerId osTimerCreate( const osTimerDef_t* timer_def, os_timer_type type, void* argument )
{
    static uint64_t s_next_timer_id = 1;
    uint64_t timer_id = s_next_timer_id++;

    auto thread = message_loop_thread_manager::get_instance()
        .get_message_loop( s_background_timer_thread->thread_id );
    bool _periodic = ( type == osTimerPeriodic );

    thread->MakeNewAlarm( timer_id, "", std::bind( timer_def->ptimer, argument ), _periodic );

    struct os_timer_cb* timer_cb = new struct os_timer_cb;
    timer_cb->id = timer_id;

    return timer_cb;
}

osStatus osTimerStart( osTimerId timer_id, uint32_t millisec )
{
    auto thread = message_loop_thread_manager::get_instance()
        .get_message_loop( s_background_timer_thread->thread_id );
    thread->SetAlarm( timer_id->id, millisec, millisec );
    return osOK;
}

osStatus osTimerStop( osTimerId timer_id )
{
    auto thread = message_loop_thread_manager::get_instance()
        .get_message_loop( s_background_timer_thread->thread_id );
    thread->SetopAlarm( timer_id->id );
    return osOK;
}

osStatus osTimerDelete( osTimerId timer_id )
{
    auto thread = message_loop_thread_manager::get_instance()
        .get_message_loop( s_background_timer_thread->thread_id );
    thread->SetopAlarm( timer_id->id );
    return osOK;
}

int32_t osSignalSet( osThreadId thread_id, int32_t signals )
{
    return thread_id->m_thread_signaling.set_value( signals );
}

int32_t osSignalClear( osThreadId thread_id, int32_t signals )
{
    return thread_id->m_thread_signaling.clear_value( signals );
}

osEvent osSignalWait( int32_t signals, uint32_t millisec )
{
    auto current = osThreadGetId();
    return current->m_thread_signaling.wait_value( signals, millisec );
}

osMutexId osMutexCreate( const osMutexDef_t* mutex_def )
{
    osMutexId mutex = new os_mutex_cb;
    return mutex;
}

osStatus osMutexWait( osMutexId mutex_id, uint32_t millisec )
{
    // 参数校验
    if( mutex_id == nullptr )
    {
        return osErrorParameter;
    }

    auto& mtx = mutex_id->m_mutex;

    if( millisec == osWaitForever )
    {
        // 永久阻塞
        mtx.lock();
        return osOK;
    }
    else if( millisec == 0U )
    {
        // 非阻塞try
        if( mtx.try_lock() )
        {
            return osOK;
        }
        return osErrorTimeoutResource;
    }
    else
    {
        // 带超时等待
        auto dur = std::chrono::milliseconds( millisec );
        if( mtx.try_lock_for( dur ) )
        {
            return osOK;
        }
        return osErrorTimeoutResource;
    }
}

osStatus osMutexRelease( osMutexId mutex_id )
{
    mutex_id->m_mutex.unlock();
    return osOK;
}

osStatus osMutexDelete( osMutexId mutex_id )
{
    delete mutex_id;
    return osOK;
}

osSemaphoreId osSemaphoreCreate( const osSemaphoreDef_t* semaphore_def, int32_t count )
{
    (void)semaphore_def; // def参数在CMSIS v1只是占位，很多实现不使用
    if( count < 0 )
    {
        return nullptr;
    }
    auto* cb = new os_semaphore_cb{
        .m_semaphore_detail = std::counting_semaphore<0xFFFFFF>( static_cast<uint32_t>( count ) ),
        .m_count = count
    };
    return cb;
}

int32_t osSemaphoreWait( osSemaphoreId semaphore_id, uint32_t millisec )
{
    if( semaphore_id == nullptr )
    {
        return -1;
    }
    auto& sem = semaphore_id->m_semaphore_detail;
    auto& cnt = semaphore_id->m_count;

    bool acquired = false;
    if( millisec == osWaitForever )
    {
        sem.acquire();
        acquired = true;
    }
    else if( millisec == 0U )
    {
        acquired = sem.try_acquire();
    }
    else
    {
        acquired = sem.try_acquire_for( std::chrono::milliseconds( millisec ) );
    }

    if( acquired )
    {
        // 拿到token，计数-1，返回新的剩余数量
        int32_t new_val = --cnt;
        return new_val;
    }
    else
    {
        // 超时，返回0（CMSIS v1规范）
        return 0;
    }
}

osStatus osSemaphoreRelease( osSemaphoreId semaphore_id )
{
    if( semaphore_id == nullptr )
    {
        return osErrorParameter;
    }
    auto& sem = semaphore_id->m_semaphore_detail;
    auto& cnt = semaphore_id->m_count;

    sem.release();
    ++cnt;
    return osOK;
}

osStatus osSemaphoreDelete( osSemaphoreId semaphore_id )
{
    if( semaphore_id == nullptr )
    {
        return osErrorParameter;
    }
    delete semaphore_id;
    return osOK;
}

osPoolId osPoolCreate( const osPoolDef_t* pool_def )
{
    if( pool_def == nullptr || pool_def->item_sz == 0U )
    {
        return nullptr;
    }
    auto* cb = new os_pool_cb();
    cb->item_size = pool_def->item_sz;
    return cb;
}

void* osPoolAlloc( osPoolId pool_id )
{
    if( pool_id == nullptr || pool_id->item_size == 0U )
    {
        return nullptr;
    }
    // 直接new，不做池化，不限制数量
    return ::operator new( pool_id->item_size );
}

void* osPoolCAlloc( osPoolId pool_id )
{
    void* p = osPoolAlloc( pool_id );
    if( p != nullptr )
    {
        memset( p, 0, pool_id->item_size );
    }
    return p;
}

osStatus osPoolFree( osPoolId pool_id, void* block )
{
    if( pool_id == nullptr || block == nullptr )
    {
        return osErrorParameter;
    }
    ::operator delete( block );
    return osOK;
}

osMessageQId osMessageCreate( const osMessageQDef_t* queue_def, osThreadId thread_id )
{
    (void)thread_id; // CMSIS v1 这个参数一般用于绑定接收线程，模拟器直接忽略
    if( queue_def == nullptr || queue_def->queue_sz == 0U )
    {
        return nullptr;
    }
    auto* cb = new os_messageQ_cb{};
    cb->m_max_count = queue_def->queue_sz;
    return cb;
}

osStatus osMessagePut( osMessageQId queue_id, uint32_t info, uint32_t millisec )
{
    if( queue_id == nullptr )
    {
        return osErrorParameter;
    }
    std::unique_lock<std::mutex> lock( queue_id->m_mtx );
    auto& q = queue_id->m_queue;
    const uint32_t max_q = queue_id->m_max_count;

    // 队列已满，需要等待空位
    if( q.size() >= max_q )
    {
        if( millisec == 0U )
        {
            return osErrorTimeoutResource;
        }
        if( millisec == osWaitForever )
        {
            queue_id->m_cv.wait( lock, [&]() { return q.size() < max_q; } );
        }
        else
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( millisec );
            bool ok = queue_id->m_cv.wait_until( lock, deadline, [&]() { return q.size() < max_q; } );
            if( !ok )
            {
                return osErrorTimeoutResource;
            }
        }
    }
    q.push( info );
    queue_id->m_cv.notify_one();
    return osOK;
}

osEvent osMessageGet( osMessageQId queue_id, uint32_t millisec )
{
    osEvent evt{};
    evt.status = osEventTimeout;
    evt.value.v = 0U;

    if( queue_id == nullptr )
    {
        evt.status = osErrorParameter;
        return evt;
    }

    std::unique_lock<std::mutex> lock( queue_id->m_mtx );
    auto& q = queue_id->m_queue;

    if( q.empty() )
    {
        if( millisec == 0U )
        {
            return evt;
        }
        if( millisec == osWaitForever )
        {
            queue_id->m_cv.wait( lock, [&]() { return !q.empty(); } );
        }
        else
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( millisec );
            bool ok = queue_id->m_cv.wait_until( lock, deadline, [&]() { return !q.empty(); } );
            if( !ok )
            {
                return evt;
            }
        }
    }

    uint32_t msg = q.front();
    q.pop();
    queue_id->m_cv.notify_one(); // 通知put方有空位
    evt.status = osEventMessage;
    evt.value.v = msg;
    return evt;
}

osMailQId osMailCreate( const osMailQDef_t* queue_def, osThreadId thread_id )
{
    (void)thread_id;
    if( queue_def == nullptr || queue_def->queue_sz == 0U || queue_def->item_sz == 0U )
    {
        return nullptr;
    }
    auto* cb = new os_mailQ_cb{};
    cb->m_max_mail = queue_def->queue_sz;
    cb->m_item_size = queue_def->item_sz;
    cb->m_used_count = 0U;
    // queue_def->pool 在模拟器直接忽略，不使用静态池
    return cb;
}

void* osMailAlloc( osMailQId queue_id, uint32_t millisec )
{
    if( queue_id == nullptr )
    {
        return nullptr;
    }
    std::unique_lock<std::mutex> lock( queue_id->m_mtx );

    if( queue_id->m_used_count >= queue_id->m_max_mail )
    {
        if( millisec == 0U )
        {
            return nullptr;
        }
        if( millisec == osWaitForever )
        {
            queue_id->m_cv_put.wait( lock, [&]() {
                return queue_id->m_used_count < queue_id->m_max_mail;
                } );
        }
        else
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( millisec );
            bool ok = queue_id->m_cv_put.wait_until( lock, deadline, [&]() {
                return queue_id->m_used_count < queue_id->m_max_mail;
                } );
            if( !ok )
            {
                return nullptr;
            }
        }
    }

    void* p = ::operator new( queue_id->m_item_size, std::nothrow );
    if( p == nullptr )
    {
        return nullptr;
    }
    queue_id->m_used_count += 1;
    return p;
}

void* osMailCAlloc( osMailQId queue_id, uint32_t millisec )
{
    void* p = osMailAlloc( queue_id, millisec );
    if( p != nullptr )
    {
        memset( p, 0, queue_id->m_item_size );
    }
    return p;
}

osStatus osMailPut( osMailQId queue_id, void* mail )
{
    if( queue_id == nullptr || mail == nullptr )
    {
        return osErrorParameter;
    }
    std::lock_guard<std::mutex> lock( queue_id->m_mtx );
    queue_id->m_mail_queue.push( mail );
    queue_id->m_cv_get.notify_one();
    return osOK;
}

osEvent osMailGet( osMailQId queue_id, uint32_t millisec )
{
    osEvent evt{};
    evt.status = osEventTimeout;
    evt.value.p = nullptr;

    if( queue_id == nullptr )
    {
        evt.status = osErrorParameter;
        return evt;
    }
    std::unique_lock<std::mutex> lock( queue_id->m_mtx );

    if( queue_id->m_mail_queue.empty() )
    {
        if( millisec == 0U )
        {
            return evt;
        }
        if( millisec == osWaitForever )
        {
            queue_id->m_cv_get.wait( lock, [&]() {
                return !queue_id->m_mail_queue.empty();
                } );
        }
        else
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( millisec );
            bool ok = queue_id->m_cv_get.wait_until( lock, deadline, [&]() {
                return !queue_id->m_mail_queue.empty();
                } );
            if( !ok )
            {
                return evt;
            }
        }
    }

    void* p = queue_id->m_mail_queue.front();
    queue_id->m_mail_queue.pop();
    queue_id->m_cv_put.notify_one();
    evt.status = osEventMail;
    evt.value.p = p;
    return evt;
}

osStatus osMailFree( osMailQId queue_id, void* mail )
{
    if( queue_id == nullptr || mail == nullptr )
    {
        return osErrorParameter;
    }
    std::lock_guard<std::mutex> lock( queue_id->m_mtx );
    ::operator delete( mail );
    queue_id->m_used_count -= 1;
    queue_id->m_cv_put.notify_one();
    return osOK;
}

void free_resource( void* ptr)
{
    free( ptr );
}

struct os_thread_cb* allocate_one_k_thread()
{
    struct os_thread_cb* ptr = (struct os_thread_cb*)malloc( sizeof( struct os_thread_cb ) );
    if( ptr )
    {
        ptr->has_onwership = 1;
        memset( ptr, 0x00, sizeof( struct os_thread_cb ) );
    }
    return ptr;
}

uint64_t get_underlying_id_for_thread( struct os_thread_cb* th)
{
    if( th )
    {
        return th->thread_id;
    }
    return 0;
}

void set_underlying_id_for_thread( struct os_thread_cb* th, uint64_t a_id )
{
    th->thread_id = a_id;
}

#ifdef __cplusplus
}
#endif

thread_signaling::thread_signaling()
{

}

int32_t thread_signaling::set_value( int32_t a_signaling_value )
{
    std::lock_guard locker( m_mutex );
    auto previous_value = m_signaling_value;
    m_signaling_value = a_signaling_value;
    m_condtion.notify_all();
    return previous_value;
}

osEvent thread_signaling::wait_value( int32_t a_signaling_value, uint64_t a_millisecs )
{
    osEvent evt{};
    evt.status = osEventTimeout;
    evt.value.signals = 0;

    std::unique_lock<std::mutex> lock( m_mutex );

    bool timed_out = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( a_millisecs );

    while( !check_signal_match( a_signaling_value ) )
    {
        if( a_millisecs == 0 )
        {
            // millisec=0：不等待，仅立即检测一次
            timed_out = true;
            break;
        }
        auto wait_ret = m_condtion.wait_until( lock, deadline );
        if( wait_ret == std::cv_status::timeout )
        {
            timed_out = true;
            break;
        }
        // 虚假唤醒，回到循环继续判断
    }

    if( !timed_out )
    {
        uint32_t hit_bits = 0;
        if( a_signaling_value == 0 )
        {
            // ANY模式：取当前全部置1的bit
            hit_bits = m_signaling_value;
        }
        else
        {
            // ALL模式：命中掩码就是传入的signals
            hit_bits = static_cast<uint32_t>( a_signaling_value );
        }

        // CMSIS v1强制：等待成功后清除命中的bit
        m_signaling_value &= ~hit_bits;

        evt.status = osEventSignal;
        evt.value.signals = static_cast<int32_t>( hit_bits );
    }

    return evt;
}

int32_t thread_signaling::clear_value( int32_t a_signaling_value )
{
    std::lock_guard locker( m_mutex );

    uint32_t mask = (uint32_t)a_signaling_value;
    uint32_t old = m_signaling_value;
    m_signaling_value = old & ( ~mask );
    return (int32_t)old;
}

bool thread_signaling::check_signal_match( int32_t a_value )
{
    if( a_value == 0 )
    {
        return m_signaling_value != 0U;
    }

    uint32_t mask = static_cast<uint32_t>( a_value );
    return ( ( m_signaling_value & mask ) == mask );
}

