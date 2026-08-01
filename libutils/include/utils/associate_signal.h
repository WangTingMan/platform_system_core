#pragma once
#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#define DEBUG_SIGNAL

#ifdef DEBUG_SIGNAL
#if __has_include(<log/log.h>)
#include <log/log.h>
#define SIGNAL_LOG ALOGI
#endif
#endif

#ifndef SIGNAL_LOG
#define SIGNAL_LOG(...)
#endif

struct user_data_type
{
    uint64_t type = 0x00;
};

/**
 * m_upstream_signals: one of these signals once triggered,
 * then this signal will go to triggered.
 * m_downstream_signals: once this signal triggered, then this signal
 * needs trigger all these ones in the m_downstream_signals.
 *
 * Public trigger semantics:
 *   trigger() is edge-triggered. If this signal is already triggered,
 *   repeated user calls are ignored until reset() is called.
 *
 * Internal propagation semantics:
 *   Cascaded trigger_dfs() calls are not suppressed by the triggered
 *   state. An already-triggered signal may notify its waiters again and
 *   continue propagating the event to its downstream signals.
 *
 * @brief This component operates on an Edge-Triggered model.
 * * @note If the signal is already in a triggered state, subsequent calls to trigger()
 * will be intercepted and ignored. To force a re-trigger, you MUST explicitly
 * invoke reset() prior to calling trigger().
 */
class associate_signal : public std::enable_shared_from_this<associate_signal>
{

public:

    associate_signal
        (
        uint64_t a_id = 0,
        std::string a_name = ""
        )
        : m_id( a_id )
        , m_name( std::move( a_name ) )
    {
    }

    uint64_t get_id() const { return m_id; }
    const std::string& get_name() const { return m_name; }

    void bind_user_data(std::shared_ptr<user_data_type> a_user_data)
    {
        std::unique_lock locker( m_mutex );
        m_user_data = a_user_data;
    }

    std::shared_ptr<user_data_type> get_bond_user_data()const
    {
        std::unique_lock locker( m_mutex );
        return m_user_data;
    }

    bool wait()
    {
        std::unique_lock locker(m_mutex);
        if( m_triggered )
        {
            return true;
        }

        m_condition.wait( locker, [ this ]() { return m_triggered; } );
        return true;
    }

    bool wait_for( std::chrono::milliseconds a_duration )
    {
        std::unique_lock locker( m_mutex );
        if( m_triggered )
        {
            return true;
        }

        bool result = m_condition.wait_for( locker, a_duration, [ this ]() { return m_triggered; } );
        return result;
    }

    bool get_status()const
    {
        std::lock_guard locker(m_mutex);
        return m_triggered;
    }

    void reset()
    {
        std::lock_guard locker( m_mutex );
        m_triggered = false;
        SIGNAL_LOG("reset %d, name %s", m_id, m_name.c_str());
    }

    void trigger()
    {
        std::unordered_set<associate_signal*> visited;
        std::unique_lock locker( m_mutex );
        if( m_triggered )
        {
            SIGNAL_LOG( "trigger %d, name %s failed, already triggered", m_id, m_name.c_str() );
            return;
        }
        locker.unlock();
        trigger_dfs(visited);
    }

    void bind_upstream( std::shared_ptr<associate_signal> a_signal )
    {
        if( !a_signal || a_signal.get() == this )
        {
            return;
        }

        bool already_bond = false;
        std::unique_lock locker(m_mutex);
        for( auto it = m_upstream_signals.begin(); it != m_upstream_signals.end(); )
        {
            auto ele = it->lock();
            if( !ele )
            {
                it = m_upstream_signals.erase(it);
                continue;
            }

            if( ele.get() == a_signal.get() )
            {
                // we already associtated.
                already_bond = true;
                break;
            }

            ++it;
        }

        if( !already_bond )
        {
            m_upstream_signals.push_back(a_signal);
        }

        locker.unlock();

        if( !already_bond )
        {
            a_signal->add_downstream( shared_from_this() );
        }

        if( a_signal->get_status() )
        {
            reset();
            trigger();
        }
    }

    void remove_upstream_signal( std::shared_ptr<associate_signal> a_signal )
    {
        if( !a_signal )
        {
            return;
        }

        std::unique_lock locker( m_mutex );
        for( auto it = m_upstream_signals.begin(); it != m_upstream_signals.end(); )
        {
            auto ele = it->lock();
            if( !ele )
            {
                it = m_upstream_signals.erase( it );
                continue;
            }

            if( ele.get() == a_signal.get() )
            {
                it = m_upstream_signals.erase( it );
                return;
            }

            ++it;
        }
    }

    void remove_downstream_signal( std::shared_ptr<associate_signal> a_signal )
    {
        if( !a_signal )
        {
            return;
        }

        std::unique_lock locker( m_mutex );
        for( auto it = m_downstream_signals.begin(); it != m_downstream_signals.end(); )
        {
            auto ele = it->lock();
            if( !ele )
            {
                it = m_downstream_signals.erase( it );
                continue;
            }

            if( ele.get() == a_signal.get() )
            {
                it = m_downstream_signals.erase( it );
                return;
            }

            ++it;
        }
    }

    void disconnect_all_signal()
    {
        auto thiz = shared_from_this();
        std::vector<std::weak_ptr<associate_signal>> _upstream_signals;
        std::vector<std::weak_ptr<associate_signal>> _downstream_signals;

        std::unique_lock locker( m_mutex );
        _upstream_signals = std::move( m_upstream_signals );
        _downstream_signals = std::move( m_downstream_signals );
        locker.unlock();

        for( auto& ele : _downstream_signals )
        {
            auto sig = ele.lock();
            if( sig )
            {
                sig->remove_upstream_signal(thiz);
            }
        }

        for( auto& ele : _upstream_signals )
        {
            auto sig = ele.lock();
            if( sig )
            {
                sig->remove_downstream_signal(thiz);
            }
        }
    }

private:

    void add_downstream( std::shared_ptr<associate_signal> a_signal )
    {
        if( !a_signal || a_signal.get() == this )
        {
            return;
        }

        std::unique_lock locker( m_mutex );
        for( auto it = m_downstream_signals.begin(); it != m_downstream_signals.end(); )
        {
            auto ele = it->lock();
            if( !ele )
            {
                it = m_downstream_signals.erase( it );
                continue;
            }

            if( ele.get() == a_signal.get() )
            {
                // we already watched.
                return;
            }

            ++it;
        }

        m_downstream_signals.push_back( a_signal );
    }

    void trigger_dfs(std::unordered_set<associate_signal*>& visited )
    {
        std::vector<std::shared_ptr<associate_signal>> signals_to_trigger;
        std::unique_lock locker( m_mutex );
        m_triggered = true;
        m_condition.notify_all();

        SIGNAL_LOG( "triggered %d, name %s", m_id, m_name.c_str() );
        for( auto it = m_downstream_signals.begin(); it != m_downstream_signals.end(); )
        {
            auto signal = it->lock();
            if( signal )
            {
                signals_to_trigger.push_back( signal );
            }
            else
            {
                it = m_downstream_signals.erase( it );
                continue;
            }
            ++it;
        }
        locker.unlock();

        for( const auto& signal : signals_to_trigger )
        {
            if( visited.contains( signal.get() ) )
            {
                continue;
            }
            visited.insert(signal.get());
            signal->trigger_dfs( visited );
        }
    }

    mutable std::recursive_mutex m_mutex;
    std::condition_variable_any m_condition;
    bool m_triggered = false;
    std::shared_ptr<user_data_type> m_user_data;
    uint64_t m_id = 0x00;
    std::string m_name;
    std::vector<std::weak_ptr<associate_signal>> m_upstream_signals;
    std::vector<std::weak_ptr<associate_signal>> m_downstream_signals;
};

