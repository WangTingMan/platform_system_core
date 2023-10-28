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

#include <string>

using namespace android;

// Interface (our AIDL) - Shared by server and client
class IDemo : public IInterface
{
public:

    enum
    {
        ALERT = IBinder::FIRST_CALL_TRANSACTION,
        COMMON_MESSAGE
    };

    virtual std::string        transaction_message( std::string const& a_parameters ) = 0;

public:

    static ::android::String16 descriptor;

    static void set_descriptor( std::string const& a_name );

    static ::android::sp<IDemo> asInterface( const ::android::sp<::android::IBinder>& obj );

    virtual const ::android::String16& getInterfaceDescriptor() const;

    IDemo();

    virtual ~IDemo();

    static bool setDefaultImpl( ::android::sp<IDemo> impl );

    static const ::android::sp<IDemo>& getDefaultImpl();

private:

    static ::android::sp<IDemo> default_impl;
};

// Client
class BpDemo : public BpInterface<IDemo>
{

public:

    BpDemo( const sp<IBinder>& impl ) : BpInterface<IDemo>( impl )
    {
    }

    virtual std::string        transaction_message( std::string const& a_parameters );

};

class BnDemo : public BnInterface<IDemo>
{

public:

    virtual status_t onTransact( uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0 );
};

class Demo : public BnDemo
{
    virtual std::string        transaction_message( std::string const& a_parameters );
};

