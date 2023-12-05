#include "binderwrapper/observer.h"

const android::String16 IObserver::descriptor( "com.bluetooth.observer" );

const android::String16& IObserver::getInterfaceDescriptor() const
{
    return IObserver::descriptor;
}

android::sp<IObserver> IObserver::asInterface( const android::sp<android::IBinder>& obj )
{
    android::sp<IObserver> intr;
    if( obj != NULL )
    {
        intr = static_cast<IObserver*>( obj->queryLocalInterface( IObserver::descriptor ).get() );
        if( intr == NULL )
        {
            intr = new BpObserver( obj );
        }
    }
    return intr;
}

IObserver::IObserver() {}

IObserver::~IObserver() {}

android::status_t BnObserver::onTransact( uint32_t code, const android::Parcel& data, android::Parcel* reply, uint32_t flags )
{
    android::status_t ret_status = android::ANDROID_NO_ERROR;
    switch( code )
    {
    case A2DP_CONNECTION_STATUS_CHANGED:
        {
            std::vector<uint8_t> addr;
            bool connected = false;
            ret_status = data.readByteVector( &addr );
            connected = data.readBool();
            A2dpConnectionStatusChanged( addr, connected );
        }
        break;
    case A2DP_ACTIVE_DEVICE_CHANGED:
        {
            std::vector<uint8_t> addr;
            ret_status = data.readByteVector( &addr );
            A2dpActiveDeviceChanged( addr );
        }
        break;
    default:
        return BBinder::onTransact( code, data, reply, flags );
    }
    return ret_status;
}

