#pragma once
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#if __has_include(<cutils/threads.h>)
#include <cutils/threads.h>
#endif

template <class _Rep, class _Period>
[[nodiscard]] auto to_absolute_time( const std::chrono::duration<_Rep, _Period>& _Rel_time ) noexcept
{
    constexpr auto _Zero = std::chrono::duration<_Rep, _Period>::zero();
    const auto _Now = std::chrono::steady_clock::now();
    decltype( _Now + _Rel_time ) _Abs_time = _Now; // return common type
    if( _Rel_time > _Zero )
    {
        constexpr auto _Forever = (std::chrono::steady_clock::time_point::max)( );
        if( _Abs_time < _Forever - _Rel_time )
        {
            _Abs_time += _Rel_time;
        }
        else
        {
            _Abs_time = _Forever;
        }
    }
    return _Abs_time;
}

class shared_timed_recursive_mutex
{
    // class for mutual exclusion shared across threads
public:

    shared_timed_recursive_mutex() = default;
    shared_timed_recursive_mutex( const shared_timed_recursive_mutex& ) = delete;
    shared_timed_recursive_mutex& operator=( const shared_timed_recursive_mutex& ) = delete;

    void lock()
    { // lock exclusive
        int cid = get_current_thread_native_id();
        do
        {
            std::unique_lock<std::mutex> _Lock( _Mymtx );
            while( !_writers.empty() )
            {
                if( std::count( _writers.begin(), _writers.end(), cid ) == _writers.size() )
                {
                    break;
                }

                _Write_queue.wait(_Lock);
            }

            while( !_readers.empty() )
            {
                auto cid_count = std::count( _readers.begin(), _readers.end(), cid );
                if( cid_count == _readers.size() )
                {
                    break;
                }
                _Read_queue.wait( _Lock );// wait for writing, no readers
            }

            if( std::count( _writers.begin(), _writers.end(), cid ) != _writers.size() ||
                std::count( _readers.begin(), _readers.end(), cid ) != _readers.size() )
            {
                // hmmm... during we wait condition _Read_queue, somebody( thread ) changed _writers...
                // So we cannot register current thread as writer! Need wait for another chance to do this.
                continue;
            }

            // OK! we can register current thread as writer now!
            _writers.push_back( cid );
            return;
        } while( true );
    }

    bool try_lock()
    { // try to lock exclusive
        int cid = get_current_thread_native_id();
        std::lock_guard<std::mutex> _Lock( _Mymtx );
        auto cid_count = std::count( _writers.begin(), _writers.end(), cid );
        if( cid_count != _writers.size() )
        {
            return false;
        }

        cid_count = std::count( _readers.begin(), _readers.end(), cid );
        if( cid_count != _readers.size() )
        {
            return false;
        }

        _writers.push_back( cid );
        return true;
    }

    template <class _Rep, class _Period>
    bool try_lock_for(
        const std::chrono::duration<_Rep, _Period>& _Rel_time )
    { // try to lock for duration
        return try_lock_until( to_absolute_time( _Rel_time ) );
    }

    template <class _Clock, class _Duration>
    bool try_lock_until( const std::chrono::time_point<_Clock, _Duration>& _Abs_time )
    {
        // try to lock until time point
#if _HAS_CXX20
        static_assert( std::chrono::is_clock_v<_Clock>, "Clock type required" );
#endif // _HAS_CXX20

        int cid = get_current_thread_native_id();
        do 
        {
            std::unique_lock<std::mutex> _Lock( _Mymtx );
            while( !_writers.empty() )
            {
                if( std::count( _writers.begin(), _writers.end(), cid ) == _writers.size() )
                {
                    break;
                }

                auto wait_result = _Write_queue.wait_until( _Lock, _Abs_time );
                if( std::cv_status::timeout  == wait_result)
                {
                    return false;
                }
            }

            while( std::count( _readers.begin(), _readers.end(), cid ) != _readers.size() )
            {
                auto wait_result = _Read_queue.wait_for( _Lock, _Abs_time );
                if( std::cv_status::timeout == wait_result )
                {
                    return false;
                }
            }

            if( std::count( _writers.begin(), _writers.end(), cid ) != _writers.size() ||
                std::count( _readers.begin(), _readers.end(), cid ) != _readers.size() )
            {
                // hmmm... during we wait condition _Read_queue, somebody( thread ) changed _writers...
                // So we cannot register current thread as writer! Need wait for another chance to do this.
                continue;
            }

            _writers.push_back( cid );
            return true;
        } while( true );

        return false;
    }

    void unlock()
    { // unlock exclusive
        { // unlock before notifying, for efficiency
            int cid = get_current_thread_native_id();
            std::lock_guard<std::mutex> _Lock( _Mymtx );
            auto cid_count = std::count( _writers.begin(), _writers.end(), cid );
            if( cid_count != _writers.size() || _writers.empty() )
            {
                std::string error_msg{"Try to unlock but not hold the mutex!"};
                throw std::runtime_error( error_msg );
            }

            _writers.pop_back();
        }

        _Write_queue.notify_all();
        _Read_queue.notify_all();
    }

    void lock_shared()
    { // lock non-exclusive
        int cid = get_current_thread_native_id();
        std::unique_lock<std::mutex> _Lock( _Mymtx );
        while( std::count(_writers.begin(), _writers.end(), cid ) != _writers.size() )
        {
            _Write_queue.wait( _Lock );
        }

        _readers.push_back( cid );
    }

    bool try_lock_shared()
    { // try to lock non-exclusive
        int cid = get_current_thread_native_id();
        std::lock_guard<std::mutex> _Lock( _Mymtx );
        if( std::count( _writers.begin(), _writers.end(), cid ) != _writers.size() )
        {
            return false;
        }

        _readers.push_back( cid );
        return true;
    }

    template <class _Rep, class _Period>
    bool try_lock_shared_for( const std::chrono::duration<_Rep, _Period>& _Rel_time )
    {
        // try to lock non-exclusive for relative time
        return try_lock_shared_until( to_absolute_time( _Rel_time ) );
    }

    template <class _Clock, class _Duration>
    bool try_lock_shared_until( const std::chrono::time_point<_Clock, _Duration>& _Abs_time )
    {
        // try to lock non-exclusive until absolute time
#if _HAS_CXX20
        static_assert( std::chrono::is_clock_v<_Clock>, "Clock type required" );
#endif // _HAS_CXX20
        int cid = get_current_thread_native_id();
        auto _Can_acquire = [this, cid]()->bool
            {
                return std::count( _writers.begin(), _writers.end(), cid ) == _writers.size();
            };

        std::unique_lock<std::mutex> _Lock( _Mymtx );
        if( _Write_queue.wait_until( _Lock, _Abs_time, _Can_acquire ) )
        {
            _readers.push_back( cid );
            return true;
        }

        return false;
    }

    void unlock_shared()
    { // unlock non-exclusive
        int cid = get_current_thread_native_id();
        std::size_t reader_cnt = 0;
        std::size_t writer_cnt = 0;
        { // unlock before notifying, for efficiency
            std::lock_guard<std::mutex> _Lock( _Mymtx );
            auto it = std::find( _readers.begin(), _readers.end(), cid );
            if( it == _readers.end() )
            {
                std::string error_msg{ "Try to unlock shared but not hold the mutex!" };
                throw std::runtime_error( error_msg );
            }

            _readers.erase( it );
            reader_cnt = _readers.size();
            writer_cnt = _writers.size();
        }

        _Read_queue.notify_all();
        if( reader_cnt == 0 )
        {
            _Write_queue.notify_all();
        }
    }

private:

    int get_current_thread_native_id()
    {
        return gettid();
    }

    std::mutex _Mymtx{};
    std::condition_variable _Read_queue{};
    std::condition_variable _Write_queue{};
    std::vector<int> _readers;
    std::vector<int> _writers;
};

