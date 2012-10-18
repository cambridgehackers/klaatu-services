/*
 */

#include "WifiDebug.h"
#include "WifiService.h"
#include "WifiStateMachine.h"

#include <cutils/properties.h>
#include <binder/Parcel.h>

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

const char *enable_key = "wifi.enabled";

WifiService::WifiService()
{
    char *flags = ::getenv("DEBUG_WIFI");

    mWifiStateMachine = new WifiStateMachine("wlan0");
    mWifiStateMachine->startRunning();

    char value[PROPERTY_VALUE_MAX];
    int len = property_get(enable_key, value, "false");
    bool enabled = (strcasecmp(value, "true") == 0);
    SetEnabled(enabled);
}

WifiService::~WifiService()
{
    // printf("%s:%d ~WifiService\n", __FILE__, __LINE__);
}

void WifiService::binderDied(const wp<IBinder>& who)
{
    Mutex::Autolock _l(mLock);
    mClients.removeItem(who);
    // printf(".......BINDER CLIENT DIED...removing client from %p (%d left)\n", this, mClients.size());
}

WifiServerClient * WifiService::getClientLocked(const sp<IWifiClient>& client)
{
    ssize_t index = mClients.indexOfKey(client->asBinder());
    if (index >= 0)
	return mClients.valueAt(index);

    WifiServerClient *wifi_client = new WifiServerClient(client);
    mClients.add(client->asBinder(),wifi_client);
    status_t err = client->asBinder()->linkToDeath(this, NULL, 0);
    SLOGW_IF(err, "WifiService::Register linkToDeath failed %d\n", err);
    // printf(".....creating new WifiServerClient %p (size=%d)....\n", this, mClients.size());
    return wifi_client;
}

// BnWifiService
void WifiService::Register(const sp<IWifiClient>& client, WifiClientFlag flags)
{
    Mutex::Autolock _l(mLock);
    getClientLocked(client)->flags = flags;

    SLOGV("^^^^^^^ REGISTER CLIENT %p flags=%u ^^^^^^\n", client.get(), flags);

    if (flags & WIFI_CLIENT_FLAG_STATE)
	client->State(mWifiStateMachine->state());
    // We don't preemptively send scandata - it's probably old anyways
    if (flags & WIFI_CLIENT_FLAG_CONFIGURED_STATIONS)
	client->ConfiguredStations(mWifiStateMachine->configdata());
    if (flags & WIFI_CLIENT_FLAG_INFORMATION)
	client->Information(mWifiStateMachine->information());
    // We don't preemptively send rssi or link speed data
}

void WifiService::SetEnabled(bool enabled)
{
    Mutex::Autolock _l(mLock);
    if (enabled) {
	mWifiStateMachine->enqueue(WifiStateMachine::CMD_LOAD_DRIVER);
	mWifiStateMachine->enqueue(WifiStateMachine::CMD_START_SUPPLICANT);
	property_set(enable_key, "true");
    }
    else {
	mWifiStateMachine->enqueue(WifiStateMachine::CMD_STOP_SUPPLICANT);
	mWifiStateMachine->enqueue(WifiStateMachine::CMD_UNLOAD_DRIVER);
	property_set(enable_key, "false");
    }
}

void WifiService::SendCommand(int command, int arg1, int arg2)
{
    Mutex::Autolock _l(mLock);
    if (command == COMMAND_START_SCAN)
	mWifiStateMachine->enqueue(WifiStateMachine::CMD_START_SCAN);
    else if (command == COMMAND_ENABLE_RSSI_POLLING)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_ENABLE_RSSI_POLL, arg1 ? 1 : 0));
    else if (command == COMMAND_ENABLE_BACKGROUND_SCAN)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_ENABLE_BACKGROUND_SCAN, arg1 ? 1 : 0));
    else if (command == COMMAND_REMOVE_NETWORK)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_REMOVE_NETWORK, arg1));
    else if (command == COMMAND_SELECT_NETWORK)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_SELECT_NETWORK, arg1));
    else if (command == COMMAND_ENABLE_NETWORK)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_ENABLE_NETWORK, arg1, arg2 ? 1 : 0));
    else if (command == COMMAND_DISABLE_NETWORK)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_DISABLE_NETWORK, arg1));
    else if (command == COMMAND_RECONNECT)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_RECONNECT));
    else if (command == COMMAND_DISCONNECT)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_DISCONNECT));
    else if (command == COMMAND_REASSOCIATE)
	mWifiStateMachine->enqueue(new Message(WifiStateMachine::CMD_REASSOCIATE));
}

void WifiService::AddOrUpdateNetwork(const ConfiguredStation& cs)
{
    Mutex::Autolock _l(mLock);
    mWifiStateMachine->enqueue(new AddOrUpdateNetworkMessage(cs));
}


void WifiService::BroadcastState(WifiState state)
{
    Mutex::Autolock _l(mLock);
//    printf("^^^^^^^ BROADCAST STATE %d for %p (%d clients) ^^^^^^\n", state, this, mClients.size());
    for (size_t i = 0 ; i < mClients.size() ; i++) {
	WifiServerClient *client = mClients.valueAt(i);
	if (client->flags & WIFI_CLIENT_FLAG_STATE)
	    client->client->State(state);
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

// ---------------------------------------------------------------------------

status_t BnWifiService::onTransact( uint32_t code, 
				    const Parcel& data, 
				    Parcel* reply, 
				    uint32_t flags )
{
    switch(code) {
    case REGISTER: {
	CHECK_INTERFACE(IWifiServer, data, reply);
	sp<IWifiClient> client = interface_cast<IWifiClient>(data.readStrongBinder());
	int32_t flags = data.readInt32();
	Register(client,static_cast<WifiClientFlag>(flags));
	return NO_ERROR;
    } break;
    case SET_ENABLED: {
	CHECK_INTERFACE(IWifiServer, data, reply);
	bool enabled = (data.readInt32() != 0);
	SetEnabled(enabled);
	return NO_ERROR;
    } break;
    case SEND_COMMAND: {
	CHECK_INTERFACE(IWifiServer, data, reply);
	int command = data.readInt32();
	int arg1    = data.readInt32();
	int arg2    = data.readInt32();
	SendCommand(command, arg1, arg2);
	return NO_ERROR;
    } break;
    case ADD_OR_UPDATE_NETWORK: {
	CHECK_INTERFACE(IWifiServer, data, reply);
	ConfiguredStation cs(data);
	AddOrUpdateNetwork(cs);
	return NO_ERROR;
    } break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(WifiService)


};  // namespace android
