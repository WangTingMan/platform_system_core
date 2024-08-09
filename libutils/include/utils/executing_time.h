#pragma once

#include <utils/utils_export.h>

#include <cstdint>
#include <chrono>
#include <list>
#include <mutex>

class UTILS_EXPORT executing_time
{

public:

    executing_time();

    static executing_time& get_instance();

    void push_time(const char* a_file, uint32_t a_line, std::chrono::microseconds a_time);

    struct executine_time_block
    {
        std::chrono::microseconds m_duration_time;
        uint32_t m_line;
        const char* m_file;

        bool operator<(executine_time_block const& right)
        {
            return m_duration_time < right.m_duration_time;
        }
    };

private:

    std::mutex m_mutex; // Protect all member vairables.
    std::list<executine_time_block> m_duration_times;
};

class UTILS_EXPORT auto_duration_computer
{

public:

    auto_duration_computer(const char* a_file, uint32_t a_line);

    ~auto_duration_computer();

    uint32_t done();

private:

    bool m_is_done = false;
    std::chrono::steady_clock::time_point m_start;
    const char* m_file;
    uint32_t m_line;
};

