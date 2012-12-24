
#include <stdio.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <binder/Parcel.h>
#include "WifiService.h"
#include "WifiStateMachine.h"

namespace android {

// ---------------------------------------------------------------------------

class WifiServerClient  
{
public:
    WifiServerClient(const sp<android::IWifiClient>& c, WifiClientFlag f=WIFI_CLIENT_FLAG_ALL)
	: client(c), flags(f) {}

    sp<android::IWifiClient> client;
    WifiClientFlag           flags;
};

// ---------------------------------------------------------------------------

static const char *enable_key = "wifi.enabled";
typedef struct {
    int command;
    int event;
} MAPTYPE;
static MAPTYPE eventmap[] = {
    {WifiService::COMMAND_START_SCAN, CMD_START_SCAN}, 
    {WifiService::COMMAND_ENABLE_RSSI_POLLING, CMD_ENABLE_RSSI_POLL}, 
    {WifiService::COMMAND_ENABLE_BACKGROUND_SCAN, CMD_ENABLE_BACKGROUND_SCAN}, 
    {WifiService::COMMAND_REMOVE_NETWORK, CMD_REMOVE_NETWORK}, 
    {WifiService::COMMAND_SELECT_NETWORK, CMD_SELECT_NETWORK}, 
    {WifiService::COMMAND_ENABLE_NETWORK, CMD_ENABLE_NETWORK}, 
    {WifiService::COMMAND_DISABLE_NETWORK, CMD_DISABLE_NETWORK}, 
    {WifiService::COMMAND_RECONNECT, CMD_RECONNECT}, 
    {WifiService::COMMAND_DISCONNECT, CMD_DISCONNECT}, 
    {WifiService::COMMAND_REASSOCIATE, CMD_REASSOCIATE}, {0,0}};

WifiService::WifiService()
{
    char value[PROPERTY_VALUE_MAX];
    char *flags = ::getenv("DEBUG_WIFI");

    mState = WS_DISABLED;
    mWifiStateMachine = new WifiStateMachine("wlan0", this);
    property_get(enable_key, value, "false");
    SetEnabled((strcasecmp(value, "true") == 0));
}

void WifiService::binderDied(const wp<IBinder>& who)
{
    Mutex::Autolock _l(mLock);
    mClients.removeItem(who);
    // printf(".......BINDER CLIENT DIED...removing client from %p (%d left)\n", this, mClients.size());
}

// BnWifiService
void WifiService::Register(const sp<IWifiClient>& client, WifiClientFlag flags)
{
    Mutex::Autolock _l(mLock);
    ssize_t index = mClients.indexOfKey(client->asBinder());
    WifiServerClient *client_for_wifi;
    if (index >= 0)
	client_for_wifi = mClients.valueAt(index);
    else {
        client_for_wifi = new WifiServerClient(client);
        mClients.add(client->asBinder(),client_for_wifi);
        status_t err = client->asBinder()->linkToDeath(this, NULL, 0);
        SLOGW_IF(err, "WifiService::etClientLocked linkToDeath failed %d\n", err);
        // printf(".....creating new WifiServerClient %p (size=%d)....\n", this, mClients.size());
    }
    client_for_wifi->flags = flags;
    SLOGV("^^^^^^^ REGISTER CLIENT %p flags=%u ^^^^^^\n", client.get(), flags);
    if (flags & WIFI_CLIENT_FLAG_STATE)
	client->State(mState);
    mWifiStateMachine->Register(client, flags);
}

void WifiService::SetEnabled(bool enabled)
{
    Mutex::Autolock _l(mLock);
    if (enabled) {
	mWifiStateMachine->enqueue(CMD_LOAD_DRIVER);
	mWifiStateMachine->enqueue(CMD_START_SUPPLICANT);
    }
    else {
	mWifiStateMachine->enqueue(CMD_STOP_SUPPLICANT);
	mWifiStateMachine->enqueue(CMD_UNLOAD_DRIVER);
    }
    property_set(enable_key, enabled ? "true" : "false");
}

void WifiService::AddOrUpdateNetwork(const ConfiguredStation& cs)
{
    Mutex::Autolock _l(mLock);
    mWifiStateMachine->enqueue_network_update(cs);
}

void WifiService::BroadcastState(WifiState state)
{
    Mutex::Autolock _l(mLock);
    if (state != mState) {
	mState = state;
//    printf("^^^^^^^ BROADCAST STATE %d for %p (%d clients) ^^^^^^\n", state, this, mClients.size());
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_STATE)
	    client->client->State(state);
    }
    }
}

void WifiService::BroadcastScanResults(const Vector<ScannedStation>& scandata)
{
//    printf("^^^^^^^ BROADCAST SCAN RESULTS (%d clients) ^^^^^^\n", mClients.size());
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_SCAN_RESULTS)
	    client->client->ScanResults(scandata);
    }
}

void WifiService::BroadcastConfiguredStations(const Vector<ConfiguredStation>& configdata)
{
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_CONFIGURED_STATIONS)
	    client->client->ConfiguredStations(configdata);
    }
}

void WifiService::BroadcastInformation(const WifiInformation& info)
{
    // printf("^^^^^^^ Calling Broadcast Information Client-size=%d\n", mClients.size());
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_INFORMATION)
	    client->client->Information(info);
    }
}

void WifiService::BroadcastRssi(int rssi)
{
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_RSSI)
	    client->client->Rssi(rssi);
    }
}

void WifiService::BroadcastLinkSpeed(int link_speed)
{
    Mutex::Autolock _l(mLock);
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_LINK_SPEED)
	    client->client->LinkSpeed(link_speed);
    }
}

void WifiService::SendCommand(int command, int arg1, int arg2)
{
    Mutex::Autolock _l(mLock);
    MAPTYPE *pmap = eventmap;

    while (pmap->event) {
        if (command == pmap->command) {
	    mWifiStateMachine->enqueue(new Message(pmap->event, arg1, arg2));
            break;
        }
        pmap++;
    }
}

// ---------------------------------------------------------------------------

status_t BnWifiService::onTransact( uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags )
{
    switch(code) {
    case REGISTER: {
	CHECK_INTERFACE(IWifiServer, data, reply);
	sp<IWifiClient> client = interface_cast<IWifiClient>(data.readStrongBinder());
	Register(client,static_cast<WifiClientFlag>(data.readInt32()));
    }   return NO_ERROR;
    case SET_ENABLED:
	CHECK_INTERFACE(IWifiServer, data, reply);
	SetEnabled((data.readInt32() != 0));
	return NO_ERROR;
    case SEND_COMMAND: {
	CHECK_INTERFACE(IWifiServer, data, reply);
	int command = data.readInt32();
	int arg1    = data.readInt32();
	int arg2    = data.readInt32();
	SendCommand(command, arg1, arg2);
    }   return NO_ERROR;
    case ADD_OR_UPDATE_NETWORK:
	CHECK_INTERFACE(IWifiServer, data, reply);
	ConfiguredStation cs(data);
	AddOrUpdateNetwork(cs);
	return NO_ERROR;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

};  // namespace android

void usage()
{
    printf("wifi - Presents a binder interface to Wifi\n"
	   "\n"
	);
    exit(0);
}

using namespace android;

int main(int argc, char **argv)
{
    if (argc > 1)
	usage();

    // The instantiate function from BinderService.h conflicts with the Singleton function
    // WifiService::instantiate();
    // So we need to do the "publishAndJoinThreadPool()" function by hand

    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16("wifi"), new WifiService());
    ProcessState::self()->startThreadPool();
    /* Add the calling thread to the IPC thread pool. This function does not return until exit. */
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
