#include "utils/executing_time.h"

#include <unordered_map>
#include <memory>
#include <shared_mutex>

class FileNameRetriever
{

public:

    const char* RetrieveFileName(const char* a_file);

    static FileNameRetriever& get_instance();

private:

    const char* RetrieveFileNameFromLocal(const char* a_file);

    std::shared_mutex m_mutex;
    std::unordered_map<const char*, const char*> m_handled_name;
};

FileNameRetriever& FileNameRetriever::get_instance()
{
    static FileNameRetriever instance;
    return instance;
}

const char* FileNameRetriever::RetrieveFileName(const char* a_file)
{
    const char* split = RetrieveFileNameFromLocal(a_file);
    if (split)
    {
        return split;
    }
    else
    {
        split = a_file;
    }

    const char* file = a_file;
    while (file && *(file++) != '\0')
    {
        if ('/' == *file || '\\' == *file)
        {
            split = file + 1;
        }
    }

    if (split)
    {
        std::lock_guard<std::shared_mutex> locker(m_mutex);
        m_handled_name[a_file] = split;
    }
    else
    {
        split = "Unkown";
    }

    return split;
}

const char* FileNameRetriever::RetrieveFileNameFromLocal(const char* a_file)
{
    std::shared_lock<std::shared_mutex> locker(m_mutex);
    auto it = m_handled_name.find(a_file);
    if (it != m_handled_name.end())
    {
        return m_handled_name[a_file];
    }
    else
    {
        return nullptr;
    }
}

executing_time::executing_time()
{

}

executing_time& executing_time::get_instance()
{
    static executing_time instance;
    return instance;
}

void executing_time::push_time(const char* a_file, uint32_t a_line, std::chrono::microseconds a_time)
{
    auto file_ = FileNameRetriever::get_instance().RetrieveFileName(a_file);

    std::lock_guard<std::mutex> locker(m_mutex);

    for (auto it = m_duration_times.begin(); it != m_duration_times.end(); ++it)
    {
        executine_time_block& ele = *it;
        if (ele.m_file == file_ && ele.m_line == a_line)
        {
            if (ele.m_duration_time < a_time)
            {
                ele.m_duration_time = a_time;
            }
            m_duration_times.sort();
            return;
        }
    }

    executine_time_block tb;
    tb.m_file = file_;
    tb.m_line = a_line;
    tb.m_duration_time = a_time;
    m_duration_times.push_back(tb);
    m_duration_times.sort();
}

auto_duration_computer::auto_duration_computer(const char* a_file, uint32_t a_line)
{
    m_file = a_file;
    m_line = a_line;
    m_start = std::chrono::steady_clock::now();
}

auto_duration_computer::~auto_duration_computer()
{
    done();
}

uint32_t auto_duration_computer::done()
{
    if (m_is_done)
    {
        return 0;
    }

    auto end_ = std::chrono::steady_clock::now();
    auto dur_ = std::chrono::duration_cast<std::chrono::microseconds>(end_ - m_start);
    executing_time::get_instance().push_time(m_file, m_line, dur_);
    m_is_done = true;
    return dur_.count();
}

