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

class LIBBINDERWRAPPER_API IObserver : public android::IInterface
{
public:

    enum
    {
        A2DP_CONNECTION_STATUS_CHANGED = android::IBinder::FIRST_CALL_TRANSACTION,
        A2DP_ACTIVE_DEVICE_CHANGED = android::IBinder::FIRST_CALL_TRANSACTION + 1
    };

    virtual void        A2dpConnectionStatusChanged( std::vector<uint8_t> a_addr, bool a_connected ) = 0;

    virtual void        A2dpActiveDeviceChanged( std::vector<uint8_t> a_addr ) = 0;

    DECLARE_META_INTERFACE( Observer );
};

class LIBBINDERWRAPPER_API BnObserver : public android::BnInterface<IObserver>
{

protected:

    virtual android::status_t onTransact( uint32_t code, const android::Parcel& data, android::Parcel* reply, uint32_t flags );

};

// Client
class LIBBINDERWRAPPER_API BpObserver : public android::BpInterface<IObserver>
{

public:

    BpObserver( const android::sp<android::IBinder>& impl ) : android::BpInterface<IObserver>( impl )
    {
    }

    virtual void        A2dpConnectionStatusChanged( std::vector<uint8_t> a_addr, bool a_connected )
    {
        android::Parcel data, reply;
        data.writeByteVector( a_addr );
        data.writeBool( a_connected );

        remote()->transact( A2DP_CONNECTION_STATUS_CHANGED, data, &reply );
    }

    virtual void        A2dpActiveDeviceChanged( std::vector<uint8_t> a_addr )
    {
        android::Parcel data, reply;
        data.writeByteVector( a_addr );
        remote()->transact( A2DP_ACTIVE_DEVICE_CHANGED, data, &reply );
    }
};

/**

class Observer : public BnObserver
{

public:

    void  A2dpConnectionStatusChanged( std::vector<uint8_t> a_addr, bool a_connected );
};

*/

