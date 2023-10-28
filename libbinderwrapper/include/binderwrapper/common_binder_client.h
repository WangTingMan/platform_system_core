#pragma once

/**
 * Warning: This is not from android opensource.
 */

#include <binderwrapper/libbinderwrapper_export.h>

#include <functional>
#include <string>
#include <thread>

class LIBBINDERWRAPPER_API common_binder_client
{

public:

    common_binder_client();

    common_binder_client( std::string const& a_service_name );

    void set_service_name( std::string const& a_service_name );

    std::string invoke_message( std::string const& a_parameters );

private:

    std::string service_name_;
};

