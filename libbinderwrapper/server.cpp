#include "binderwrapper/service.h"

const android::String16 IBluetoothService::descriptor( "com.bluetooth.service" );

const android::String16& IBluetoothService::getInterfaceDescriptor() const
{
    return IBluetoothService::descriptor;
}

android::sp<IBluetoothService> IBluetoothService::asInterface( const android::sp<android::IBinder>& obj )
{
    android::sp<IBluetoothService> intr;
    if( obj != NULL )
    {
        intr = static_cast<IBluetoothService*>( obj->queryLocalInterface( IBluetoothService::descriptor ).get() );
        if( intr == NULL )
        {
            intr = new BpBluetoothService( obj );
        }
    }
    return intr;
}

IBluetoothService::IBluetoothService() {}

IBluetoothService::~IBluetoothService() {}

android::status_t BnBluetoothService::onTransact( uint32_t code, const android::Parcel& data, android::Parcel* reply, uint32_t flags )
{
    android::status_t ret_status = android::ANDROID_NO_ERROR;
    switch( code )
    {
    case REGISTER_OBSERVER:
        {
            android::sp<IBinder> val;
            ret_status = data.readStrongBinder( &val );
            ::android::sp<IObserver> observer;
            observer = IObserver::asInterface( val );
            RegisterObserver( observer );
        }
        break;
    default:
        return BBinder::onTransact( code, data, reply, flags );
    }
    return ret_status;
}

