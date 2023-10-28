#pragma once

/**
 * Warning: This is not from android opensource.
 */

#include <binderwrapper/libbinderwrapper_export.h>

#include <functional>
#include <string>
#include <thread>

using service_handler = std::function<std::string( std::string const& )>;

class LIBBINDERWRAPPER_API common_binder_server
{

public:

    friend class Demo;

    static common_binder_server& get_instance();

    void register_handler( service_handler a_handler )
    {
        m_handler = a_handler;
    }

    void init( std::string a_local_name );

    void run( bool a_occupy_current_thread = false );

private:

    common_binder_server();

    void run_internal();

    service_handler m_handler;
    std::thread m_running_thread;
};

