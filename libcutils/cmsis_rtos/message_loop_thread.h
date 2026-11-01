/*
 * Copyright 2018 The Android Open Source Project
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

#ifdef _MSC_VER
#define NOMINMAX
#endif

#include <base/functional/bind.h>
#include <base/location.h>
#include <base/run_loop.h>
#include <base/threading/platform_thread.h>
#include <base/message_loop/message_loop.h>
#include <base/timer/timer.h>

#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>

namespace bluetooth {

namespace common {

enum class tackable_task_status
{
    not_scheduled,
    cancelled,
    scheduled,
    work_done,
};

class trackable_task_control_block
{

public:

    uint64_t get_id()const
    {
        return m_track_id;
    }

    void set_id( uint64_t a_id )
    {
        m_track_id = a_id;
    }

    void set_task( std::function<void()> a_timeout_callbac )
    {
        m_timeout_callback = a_timeout_callbac;
    }

    void set_delay_time( std::chrono::microseconds a_delay )
    {
        m_scheduled_time_point = std::chrono::steady_clock::now();
        m_delay_time = a_delay;
    }

    void handle_task()
    {
        if( m_status == tackable_task_status::work_done ||
            m_status == tackable_task_status::cancelled ||
            m_status == tackable_task_status::not_scheduled )
        {
            return;
        }

        if( m_timeout_callback )
        {
            m_timeout_callback();
        }
        m_status = tackable_task_status::work_done;
    }

    bool need_delete()
    {
        switch( m_status )
        {
        case tackable_task_status::cancelled:
            return true;
        case tackable_task_status::work_done:
            return true;
        default:
            return false;
        }
        return false;
    }

private:

    friend class MessageLoopThread;
    std::chrono::steady_clock::time_point m_scheduled_time_point;
    std::chrono::microseconds m_delay_time;
    uint64_t m_track_id = 0x00;
    tackable_task_status m_status = tackable_task_status::not_scheduled;
    base::OneShotTimer m_timer;
    std::function<void()> m_timeout_callback;
};

struct TimerControlBlock
{
    std::string m_name;
    uint32_t m_id{ 0 };
    uint32_t m_intervalms{ 0 };
    bool m_periodic{ false };
    base::RepeatingTimer m_timer;
    std::function<void()> m_callback;
};

/**
 * An interface to various thread related functionality
 */
class MessageLoopThread
{
public:
  /**
   * Create a message loop thread with name. Thread won't be running until
   * StartUp is called.
   *
   * @param thread_name name of this worker thread
   */
  explicit MessageLoopThread(const std::string& thread_name);

  MessageLoopThread(const MessageLoopThread&) = delete;
  MessageLoopThread& operator=(const MessageLoopThread&) = delete;

  /**
   * Destroys the message loop thread automatically when it goes out of scope
   */
  ~MessageLoopThread();

  /**
   * Start the underlying thread. Blocks until all thread infrastructure is
   * setup. IsRunning() and DoInThread() should return true after this call.
   * Blocks until the thread is successfully started.
   *
   * Repeated call to this method will only start this thread once
   */
  void StartUp();

  bool DoInThread(std::function<void()> a_task);

  /**
   * Post a task to run on this thread
   *
   * @param task task created through base::Bind()
   * @return true if task is successfully scheduled, false if task cannot be
   * scheduled
   */
  bool DoInThread(base::OnceClosure task);

  /**
   * Shutdown the current thread as if it is never started. IsRunning() and
   * DoInThread() will return false after this call. Blocks until the thread is
   * joined and freed. This thread can be re-started again using StartUp()
   *
   * Repeated call to this method will only stop this thread once
   *
   * NOTE: Should never be called on the thread itself to avoid deadlock
   */
  void ShutDown();

  /**
   * Get the current thread ID returned by PlatformThread::CurrentId()
   *
   * On Android platform, this value should be the same as the tid logged by
   * logcat, which is returned by gettid(). On other platform, this thread id
   * may have different meanings. Therefore, this ID is only good for logging
   * and thread comparison purpose
   *
   * @return this thread's ID
   */
  base::PlatformThreadId GetThreadId() const;

  /**
   * Check if the current thread in use is same as this thread.
   * Note: This is only valid when flag replace_message_loop_thread_with_gd_handler is enabled.
   *
   * @return true if the current thread in use is same as this thread.
   */
  bool IsRunningOnSameThread() const;

  /**
   * Get this thread's name set in constructor
   *
   * @return this thread's name set in constructor
   */
  std::string GetName() const;

  /**
   * Get a string representation of this thread
   *
   * @return a string representation of this thread
   */
  std::string ToString() const;

  /**
   * Check if this thread is running
   *
   * @return true iff this thread is running and is able to do task
   */
  bool IsRunning() const;

  /**
   * Attempt to make scheduling for this thread real time
   *
   * @return true on success, false otherwise
   */
  bool EnableRealTimeScheduling();

  /**
   * Return the weak pointer to this object. This can be useful when posting
   * delayed tasks to this MessageLoopThread using Timer.
   */
  base::WeakPtr<MessageLoopThread> GetWeakPtr();

  /**
   * Return the message loop for this thread. Accessing raw message loop is not
   * recommended as message loop can be freed internally.
   *
   * @return message loop associated with this thread, nullptr if thread is not
   * running
   */
  base::MessageLoop* message_loop() const;

  /**
   * Post a task to run on this thread after a specified delay. If the task
   * needs to be cancelable before it's run, use base::CancelableClosure type
   * for task closure. For example:
   * <code>
   * base::CancelableClosure cancelable_task;
   * cancelable_task.Reset(base::Bind(...)); // bind the task
   * same_thread->DoInThreadDelayed(cancelable_task.callback(), delay);
   * ...
   * // Cancel the task closure
   * same_thread->DoInThread(base::Bind(&base::CancelableClosure::Cancel,
   *                         base::Unretained(&cancelable_task)));
   * </code>
   *
   * Warning: base::CancelableClosure objects must be created on, posted to,
   * cancelled on, and destroyed on the same thread.
   *
   * @param task task created through base::Bind()
   * @param delay delay for the task to be executed
   * @return true if task is successfully scheduled, false if task cannot be
   * scheduled
   */
  bool DoInThreadDelayed(base::OnceClosure task, std::chrono::microseconds delay);

  bool DoInThreadDelayed( std::function<void()> task, std::chrono::microseconds delay );

  bool DoInThreadDelayedWithTrack( uint64_t a_id, std::function<void()> task, std::chrono::microseconds delay );

  bool ChangeDelayedWithTrackDelay(uint64_t a_id, std::chrono::microseconds delay );

  bool CancelDelayWithTrack(uint64_t a_id);

  bool TackedTaskID( uint64_t a_id )
  {
    std::lock_guard locker( api_mutex_ );
    return m_trackable_tasks.contains(a_id);
  }

  void MakeNewAlarm
    (
      uint64_t a_id,
      std::string&& a_name,
      std::function<void()> a_callBack,
      bool a_periodic
    );

  void SetAlarm
    (
    uint64_t a_alarm,
    uint64_t _duration,
    uint64_t a_interval_ms
    );

    void SetopAlarm(uint64_t a_id);

  /**
   * Wrapper around DoInThread without a location.
   */
  void Post(base::OnceClosure closure);

private:
  /**
   * Static method to run the thread
   *
   * This is used instead of a C++ lambda because of the use of std::shared_ptr
   *
   * @param context needs to be a pointer to an instance of MessageLoopThread
   * @param start_up_promise a std::promise that is used to notify calling
   * thread the completion of message loop start-up
   */
  static void RunThread(MessageLoopThread* context, std::promise<void> start_up_promise);

  bool DoInThreadDelayedWithTrackDetail( uint64_t a_id );

  bool ChangeDelayedWithTrackDelayDetail( uint64_t a_id );

  bool CancelDelayWithTrackDetail( uint64_t a_id );

  void SetAlarmDetail
    (
    uint64_t a_alarm,
    uint64_t a_interval_ms
    );

  void StopAlarmInternal( uint64_t a_alarm );

  void TimerEvent( uint64_t a_alarm );

  std::shared_ptr<TimerControlBlock> FindTimer( uint64_t a_alarm )
  {
      for( auto& ele : m_timers )
      {
          if( ele->m_id == a_alarm )
          {
              return ele;
          }
      }

      return nullptr;
  }

  /**
   * Actual method to run the thread, blocking until ShutDown() is called
   *
   * @param start_up_promise a std::promise that is used to notify calling
   * thread the completion of message loop start-up
   */
  void Run(std::promise<void> start_up_promise);

  mutable std::recursive_mutex api_mutex_;
  const std::string thread_name_;
  base::MessageLoop* message_loop_;
  base::RunLoop* run_loop_;
  std::thread* thread_;
  base::PlatformThreadId thread_id_;
  base::WeakPtrFactory<MessageLoopThread> weak_ptr_factory_;
  bool shutting_down_;
  std::map<uint64_t,std::shared_ptr<trackable_task_control_block>> m_trackable_tasks;
  std::list<std::shared_ptr<TimerControlBlock>> m_timers;
};

inline std::ostream& operator<<
    (
    std::ostream& os,
    const bluetooth::common::MessageLoopThread& a
    )
{
  os << a.ToString();
  return os;
}

}  // namespace common
}  // namespace bluetooth

struct os_thread_cb;

class message_loop_thread_manager
{

public:

    static message_loop_thread_manager& get_instance();

    uint64_t create_new_message_loop(std::string a_name);

    std::shared_ptr<bluetooth::common::MessageLoopThread> get_message_loop( uint64_t );

    std::shared_ptr<bluetooth::common::MessageLoopThread> get_message_loop_by_track_id( uint64_t );

    void quit_message_loop( uint64_t );

    std::shared_ptr<os_thread_cb> get_thread_cb(uint64_t a_tid);

    void keep_thread_cb( std::shared_ptr<os_thread_cb> cb )
    {
        std::lock_guard locker(m_mtex);
        m_k_thread_cbs.push_back(cb);
    }

    uint64_t get_next_tackable_id()
    {
        std::lock_guard locker( m_mtex );
        uint64_t id = m_next_tackable_task_id++;
        if( id == 0x00 )
        {
            id = m_next_tackable_task_id++;
        }
        return id;
    }

    void set_timer_duration_in_thread( uint64_t timer_id, int duration, int  _peroid_milliseconds );

    void stop_timer_in_thread( uint64_t timer_id );

private:

    std::mutex m_mtex;
    std::map<uint64_t, std::shared_ptr<bluetooth::common::MessageLoopThread>> m_threads;
    std::vector<std::shared_ptr<os_thread_cb>> m_k_thread_cbs;
    uint64_t m_next_tackable_task_id = 1; /* should not be zero any way! */
};
