#pragma once

#include <functional>

class AutoHolder
{

public:

    AutoHolder
        (
        std::function<void()> a_fun_ctr_call,
        std::function<void()> a_fun_dctr_call
        )
        : m_fun_dctr_call(a_fun_dctr_call)
    {
        a_fun_ctr_call();
    }

    AutoHolder
        (
        std::function<void()> a_fun_dctr_call
        )
        : m_fun_dctr_call( a_fun_dctr_call )
    {
    }

    ~AutoHolder()
    {
        m_fun_dctr_call();
    }

    AutoHolder( AutoHolder const& ) = delete;

    AutoHolder& operator=( AutoHolder const& ) = delete;

private:

    std::function<void()> m_fun_dctr_call;
};

