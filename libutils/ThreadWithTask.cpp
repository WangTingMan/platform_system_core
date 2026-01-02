#include <utils/ThreadWithTask.h>
#include <cutils/threads.h>

#include <future>

#include <base/threading/platform_thread.h>

ThreadWithTask::ThreadWithTask()
{

}

void ThreadWithTask::StartUp()
{
    std::promise<void> start_up_promise;
    std::future<void> start_up_future = start_up_promise.get_future();
    {
        std::lock_guard<std::recursive_mutex> api_lock( m_messageMutex );
        if (m_running_thread.joinable())
        {
            return;
        }

        m_running_thread = std::thread( &ThreadWithTask::DetailRunning, this,
            std::move( start_up_promise ) );
    }
    start_up_future.wait();
}

void ThreadWithTask::ShutDown()
{
    m_running = false;
    PostTask( []() {} );
    m_running_thread.join();
}

int ThreadWithTask::GetTid()
{
    std::lock_guard locker( m_messageMutex );
    return m_tid;
}

void ThreadWithTask::SetName( std::string a_name )
{
    std::lock_guard locker( m_messageMutex );
    m_name = a_name;
    if (m_running_thread.joinable())
    {
        PostTask( std::bind( &ThreadWithTask::SetNameDetail, this ) );
    }
}

void ThreadWithTask::PostTask( std::function<void()> a_task )
{
    std::lock_guard locker( m_messageMutex );
    m_tasks.push_back( a_task );
    m_notifyCondition.notify_all();
}

void ThreadWithTask::DetailRunning( std::promise<void> a_promise )
{
    SetNameDetail();
    a_promise.set_value();
    int ret = 0;
    std::vector<std::function<void()>> msgs;
    std::unique_lock<std::recursive_mutex> locker( m_messageMutex, std::defer_lock );
    m_running = true;

    m_tid = gettid();

    while (m_running)
    {
        locker.lock();
        if (m_tasks.empty())
        {
            m_notifyCondition.wait( locker, [this]()
                {
                    return !m_tasks.empty();
                }
            );
        }
        msgs = std::move( m_tasks );
        locker.unlock();

        if (m_running == false)
        {
            return;
        }

        for (auto&& ele : msgs)
        {
            if (m_running == false)
            {
                return;
            }

            if (ele)
            {
                ele();
            }
        }
    }
}

void ThreadWithTask::SetNameDetail()
{
    base::PlatformThread::SetName( m_name );
}
