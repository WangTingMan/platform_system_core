#include <binderwrapper/common_binder_server.h>
#include <binderwrapper/binder_wrapper.h>
#include <base/logging.h>
#include <base/threading/platform_thread.h>

#include "common_binder_internal.h"

common_binder_server::common_binder_server()
{

}

common_binder_server& common_binder_server::get_instance()
{
    static common_binder_server instance;
    return instance;
}

void common_binder_server::init( std::string a_local_name )
{
    if( a_local_name.empty() )
    {
        LOG( ERROR ) << "Cannot register with empty name!";
        return;
    }

    IDemo::set_descriptor( a_local_name );

    auto wrapper = android::BinderWrapper::GetOrCreateInstance();
    wrapper->RegisterService( a_local_name, new Demo );
}

void common_binder_server::run( bool a_occupy_current_thread )
{
    if( a_occupy_current_thread )
    {
        run_internal();
        return;
    }

    if( m_running_thread.joinable() )
    {
        LOG( ERROR ) << "Already running.";
        return;
    }

    m_running_thread = std::thread( &common_binder_server::run_internal, this );
}

void common_binder_server::run_internal()
{
    android::ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
}

