#include <binderwrapper/common_binder_client.h>
#include <binderwrapper/binder_wrapper.h>
#include <base/logging.h>
#include <base/threading/platform_thread.h>

#include "common_binder_internal.h"

common_binder_client::common_binder_client()
{

}

common_binder_client::common_binder_client( std::string const& a_service_name )
{
    set_service_name( a_service_name );
}

void common_binder_client::set_service_name( std::string const& a_service_name )
{
    service_name_ = a_service_name;
}

std::string common_binder_client::invoke_message( std::string const& a_parameters )
{
    auto wrapper = android::BinderWrapper::GetOrCreateInstance();
    sp<IBinder> binder = wrapper->GetService( service_name_ );
    if( !binder.get() )
    {
        LOG( ERROR ) << "Cannot find such service: " << service_name_;
        return "";
    }

    sp<IDemo> demo = interface_cast<IDemo>( binder );
    return demo->transaction_message( a_parameters );
}

