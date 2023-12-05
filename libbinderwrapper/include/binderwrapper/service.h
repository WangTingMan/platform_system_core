#pragma once
#include <stdlib.h>

#include <utils/RefBase.h>
#include <utils/Log.h>
#include <binder/TextOutput.h>

#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>

#include <base/logging.h>

#include <vector>
#include <cstdint>

#include <binderwrapper/libbinderwrapper_export.h>
#include <binderwrapper/observer.h>

class LIBBINDERWRAPPER_API IBluetoothService : public android::IInterface
{
public:

    enum
    {
        REGISTER_OBSERVER = android::IBinder::FIRST_CALL_TRANSACTION,
    };

    virtual void        RegisterObserver( android::sp<IObserver> a_observer ) = 0;

    DECLARE_META_INTERFACE( BluetoothService );
};

class LIBBINDERWRAPPER_API BnBluetoothService : public android::BnInterface<IBluetoothService>
{

protected:

    virtual android::status_t onTransact( uint32_t code, const android::Parcel& data, android::Parcel* reply, uint32_t flags );

};

// Client
class LIBBINDERWRAPPER_API BpBluetoothService : public android::BpInterface<IBluetoothService>
{

public:

    BpBluetoothService( const android::sp<android::IBinder>& impl ) : android::BpInterface<IBluetoothService>( impl )
    {
    }

    void        RegisterObserver( android::sp<IObserver> a_observer )
    {
        android::Parcel data, reply;
        data.writeStrongBinder( a_observer );

        remote()->transact( REGISTER_OBSERVER, data, &reply );
    }
};

/*
class BluetoothService : public BnBluetoothService
{

public:

    void        RegisterObserver( android::sp<IObserver> a_observer );
};
*/

