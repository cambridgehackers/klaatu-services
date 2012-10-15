/*
  SigynService 
 */

#include <binder/Parcel.h>
#include <sigyn/ISigynService.h>
#include <sigyn/ISigynClient.h>


namespace android {

class BpSigynService : public BpInterface<ISigynService>
{
public:
    BpSigynService(const sp<IBinder>& impl)
	: BpInterface<ISigynService>(impl)
    {
    }

    void Register(const sp<ISigynClient>& client, UnsolicitedMessages flags) {
	Parcel data, reply;
	data.writeInterfaceToken(ISigynService::getInterfaceDescriptor());
	data.writeStrongBinder(client->asBinder());
	data.writeInt32(flags);
	remote()->transact(REGISTER, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void Request(const sp<ISigynClient>& client, int token, int message, int ivalue, const String16& svalue) {
	Parcel data, reply;
	data.writeInterfaceToken(ISigynService::getInterfaceDescriptor());
	data.writeStrongBinder(client->asBinder());
	data.writeInt32(token);
	data.writeInt32(message);
	data.writeInt32(ivalue);
	data.writeString16(svalue);
	remote()->transact(REQUEST, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

IMPLEMENT_META_INTERFACE(SigynService, "nokia.platform.ISigynService")

// ----------------------------------------------------------------------------
}; // namespace Android
