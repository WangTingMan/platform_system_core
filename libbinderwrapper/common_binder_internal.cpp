#include "common_binder_internal.h"
#include <binderwrapper/common_binder_server.h>

android::String16 IDemo::descriptor( "Demo" );

const android::String16& IDemo::getInterfaceDescriptor() const
{
    return descriptor;
}

void IDemo::set_descriptor( std::string const& a_name )
{
    android::String16 descrp( a_name.c_str() );
    descriptor = descrp;
}

android::sp<IDemo> IDemo::asInterface( const android::sp<android::IBinder>& obj )
{
    android::sp<IDemo> intr;
    if( obj != NULL )
    {
        intr = static_cast<IDemo*>( obj->queryLocalInterface( descriptor ).get() );
        if( intr == NULL )
        {
            intr = new BpDemo( obj );
        }
    }
    return intr;
}

IDemo::IDemo() {}

IDemo::~IDemo() {}

std::string BpDemo::transaction_message( std::string const& a_parameters )
{
    Parcel data, reply;
    status_t status = ANDROID_NO_ERROR;
    data.writeUint32( a_parameters.size() );
    data.write( a_parameters.data(), a_parameters.size() );

    status = remote()->transact( COMMON_MESSAGE, data, &reply );
    if( status != ANDROID_NO_ERROR )
    {
        LOG( ERROR ) << "transaction failed with error code: " << status;
        return "";
    }

    uint32_t size_ret = 0;
    status = reply.readUint32( &size_ret );
    if( status != ANDROID_NO_ERROR )
    {
        LOG( ERROR ) << "read returned string size with error code: " << status;
        return "";
    }

    std::string ret;
    ret.resize( size_ret + 1, '\0' );
    status = reply.read( ret.data(), size_ret );
    if( status != ANDROID_NO_ERROR )
    {
        LOG( ERROR ) << "read returned string with error code: " << status;
        return "";
    }

    return ret;
}

status_t BnDemo::onTransact( uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags )
{
    switch( code )
    {
    case COMMON_MESSAGE:
        {
            uint32_t str_size = 0;
            status_t status = ANDROID_NO_ERROR;
            status = data.readUint32( &str_size );
            if( status != ANDROID_NO_ERROR )
            {
                LOG( ERROR ) << "read parameter string size with error code: " << status;
                return BAD_VALUE;
            }

            std::string parameter_str;
            parameter_str.resize( str_size + 1, '\0' );
            status = data.read( parameter_str.data(), str_size );
            if( status != ANDROID_NO_ERROR )
            {
                LOG( ERROR ) << "read parameter string with error code: " << status;
                return BAD_VALUE;
            }

            std::string ret_str;
            ret_str = transaction_message( parameter_str );
            reply->writeUint32( ret_str.size() );
            reply->write( ret_str.data(), ret_str.size() );
            return NO_ERROR;
        }
        break;
    default:
        return BBinder::onTransact( code, data, reply, flags );
    }

    return NO_ERROR;
}

std::string Demo::transaction_message( std::string const& a_parameters )
{
    std::string ret;
    auto& handler = common_binder_server::get_instance().m_handler;
    ret = handler( a_parameters );
    return ret;
}

