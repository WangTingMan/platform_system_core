/**
 * There is no such component in Android source.
 * We just add it here for useful
 */
#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include <utils/utils_export.h>

class UTILS_EXPORT ThreadWithTask
{

public:

    ThreadWithTask();

    ThreadWithTask( std::string a_name )
        : m_name( a_name )
    {

    }

    void StartUp();

    void ShutDown();

    int GetTid();

    void SetName( std::string a_name );

    void PostTask( std::function<void()> a_task );

private:

    void DetailRunning( std::promise<void> );

    void SetNameDetail();

    std::recursive_mutex m_messageMutex;
    std::string m_thread_name_;
    std::condition_variable_any m_notifyCondition;
    std::vector<std::function<void()>> m_tasks;
    bool m_running = false;
    int m_tid = 0;
    std::string m_name;

    std::thread m_running_thread;
};

