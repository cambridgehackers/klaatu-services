/*
  HeimdService 
 */

#include <binder/Parcel.h>
#include <heimd/IHeimdService.h>
#include <heimd/IHeimdClient.h>

namespace android {

class BpHeimdService : public BpInterface<IHeimdService>
{
public:
    BpHeimdService(const sp<IBinder>& impl)
	: BpInterface<IHeimdService>(impl)
    {
    }

    void Register(const sp<IHeimdClient>& client, HeimdClientFlag flags) {
	Parcel data, reply;
	data.writeInterfaceToken(IHeimdService::getInterfaceDescriptor());
	data.writeStrongBinder(client->asBinder());
	data.writeInt32(flags);
	remote()->transact(REGISTER, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void SetEnabled(bool enabled) {
	Parcel data, reply;
	data.writeInterfaceToken(IHeimdService::getInterfaceDescriptor());
	data.writeInt32(enabled);
	remote()->transact(SET_ENABLED, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void SendCommand(int command, int arg1, int arg2) {
	Parcel data, reply;
	data.writeInterfaceToken(IHeimdService::getInterfaceDescriptor());
	data.writeInt32(command);
	data.writeInt32(arg1);
	data.writeInt32(arg2);
	remote()->transact(SEND_COMMAND, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void AddOrUpdateNetwork(const ConfiguredStation& cs) {
	Parcel data, reply;
	data.writeInterfaceToken(IHeimdService::getInterfaceDescriptor());
	cs.writeToParcel(&data);
	remote()->transact(ADD_OR_UPDATE_NETWORK, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

IMPLEMENT_META_INTERFACE(HeimdService, "nokia.platform.IHeimdService")

// ----------------------------------------------------------------------------
}; // namespace Android
