
#include <stdio.h>
#include <cutils/sockets.h>
// Contains the UID of the radio process
#include <private/android_filesystem_config.h>

#include "PhoneService.h"
#include "PhoneStateMachine.h"

namespace android {

// ---------------------------------------------------------------------------

class PhoneServerClient  
{
public:
    PhoneServerClient(const sp<android::IPhoneClient>& c, UnsolicitedMessages f)
	: client(c), flags(f) {}

    sp<android::IPhoneClient> client;
    UnsolicitedMessages       flags;
};

/*
  Lookup the PhoneServerClient.  If it doesn't exist, create a new empty one.
  You must hold the mLock before calling this method
 */
PhoneServerClient * PhoneService::getClient(const sp<IPhoneClient>& client)
{
    ssize_t index = mClients.indexOfKey(client->asBinder());
    if (index >= 0)
	return mClients.valueAt(index);

    SLOGV(".....creating new PhoneServerClient....\n");
    PhoneServerClient *phone_client = new PhoneServerClient(client,UM_NONE);
    mClients.add(client->asBinder(),phone_client);
    status_t err = client->asBinder()->linkToDeath(this, NULL, 0);
    if (err)
	SLOGW("PhoneService::Register linkToDeath failed %d\n", err);
    return phone_client;
}

// Methods used from actual service
void PhoneService::broadcastUnsolicited(UnsolicitedMessages flags, int message, int ivalue, String16 svalue)
{
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mClients.size() ; i++) {
        PhoneServerClient *client = mClients.valueAt(i);
        if (client->flags & flags)
        client->client->Unsolicited(message, ivalue, svalue);
    }
}

PhoneService::PhoneService()
{
    mMachine = new PhoneMachine(this);
}

PhoneService::~PhoneService()
{
    SLOGV("%s:%d  ~Phoneservice\n",__FILE__,__LINE__);
}

void PhoneService::binderDied(const wp<IBinder>& who)
{
    SLOGV("binderDied\n");
    Mutex::Autolock _l(mLock);
    mClients.removeItem(who);
    // ### TODO: Remove all messages from the outgoing queue
}

// BnPhoneService methods

void PhoneService::Register(const sp<IPhoneClient>& client, UnsolicitedMessages flags)
{
    SLOGV("Client register flags=%0x\n", flags);

    Mutex::Autolock _l(mLock);
    getClient(client)->flags = flags;
}

void PhoneService::Request(const sp<IPhoneClient>& client, int token, int message, int ivalue, const String16& svalue) 
{
    // Add the client if it doesn't exist
    {
        Mutex::Autolock _l(mLock);
        getClient(client);
    }
    mMachine->Request(client, token, message, ivalue, svalue);
}

// ---------------------------------------------------------------------------

status_t BnPhoneService::onTransact( uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags )
{
    switch(code) {
    case REGISTER: {
	CHECK_INTERFACE(IPhoneSession, data, reply);
	sp<IPhoneClient> client = interface_cast<IPhoneClient>(data.readStrongBinder());
	int32_t myFlags = data.readInt32();
	Register(client,static_cast<UnsolicitedMessages>(myFlags));
	return NO_ERROR;
    } break;
    case REQUEST: {
	CHECK_INTERFACE(IPhoneSession, data, reply);
	sp<IPhoneClient> client = interface_cast<IPhoneClient>(data.readStrongBinder());
	int token   = data.readInt32();
	int message = data.readInt32();
	int ivalue  = data.readInt32();
	String16 svalue = data.readString16();
	Request(client, token, message, ivalue, svalue);
	return NO_ERROR;
    } break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

};  // namespace android

void usage()
{
    printf("phoned - Presents a binder interface to the RIL daemon\n\n"
	   "The phoned process needs to be able to open the /dev/socket/rild.\n"
	   "The easiest way is to run as root or give it membership in the\n"
	   "radio (1001) group\n\n"
	   "Setting the DEBUG_PHONE environment variable to 'in', 'out' or 'in:out'\n"
	   "will display extra information about messages over the RIL socket\n"
	);
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc > 1)
	usage();
    printf("Process uid=%d gid=%d\n", getuid(), getgid());
    if (getgid() != AID_RADIO && getuid() != 0) {
	if (setgid(AID_RADIO)) {
	    perror("Unable to set process GID to radio");
	    return 1;
	}
    }
    android::PhoneService::publishAndJoinThreadPool();
    return 0;
}
