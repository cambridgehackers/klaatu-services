/*
 */

#include <wifi/WifiClient.h>
#include <binder/BinderService.h>

namespace android {

void WifiClient::Register(WifiClientFlag flags)
{
    mWifiService->Register(this, flags);
}

void WifiClient::SetEnabled(bool enable)
{
    mWifiService->SetEnabled(enable);
}

void WifiClient::StartScan(bool force_active)
{
    mWifiService->SendCommand(IWifiService::COMMAND_START_SCAN, force_active, 0);
}

void WifiClient::EnableRssiPolling(bool enable)
{
    mWifiService->SendCommand(IWifiService::COMMAND_ENABLE_RSSI_POLLING, enable, 0);
}

void WifiClient::EnableBackgroundScan(bool enable)
{
    mWifiService->SendCommand(IWifiService::COMMAND_ENABLE_BACKGROUND_SCAN, enable, 0);
}

void WifiClient::AddOrUpdateNetwork(const ConfiguredStation& cs)
{
    mWifiService->AddOrUpdateNetwork(cs);
}

void WifiClient::RemoveNetwork(int network_id)
{
    mWifiService->SendCommand(IWifiService::COMMAND_REMOVE_NETWORK, network_id, 0);
}

void WifiClient::SelectNetwork(int network_id)
{
    mWifiService->SendCommand(IWifiService::COMMAND_SELECT_NETWORK, network_id, 0);
}

void WifiClient::EnableNetwork(int network_id, bool disable_others)
{
    mWifiService->SendCommand(IWifiService::COMMAND_ENABLE_NETWORK, 
			       network_id, disable_others);
}

void WifiClient::DisableNetwork(int network_id)
{
    mWifiService->SendCommand(IWifiService::COMMAND_DISABLE_NETWORK, network_id, 0);
}

void WifiClient::Reconnect()
{
    mWifiService->SendCommand(IWifiService::COMMAND_RECONNECT, 0, 0);
}

void WifiClient::Disconnect()
{
    mWifiService->SendCommand(IWifiService::COMMAND_DISCONNECT, 0, 0);
}

void WifiClient::Reassociate()
{
    mWifiService->SendCommand(IWifiService::COMMAND_REASSOCIATE, 0, 0);
}

void WifiClient::onFirstRef()
{
    sp<IServiceManager> sm     = defaultServiceManager();
    sp<IBinder>         binder = sm->getService(String16("wifi"));

    if (!binder.get()) {
	fprintf(stderr, "Unable to connect to wifi service\n");
	exit(-1);
    }
    status_t err = binder->linkToDeath(this, 0);
    if (err) {
	fprintf(stderr, "Unable to link to wifi death: %s", strerror(-err));
	exit(-1);
    }
 
    mWifiService = interface_cast<IWifiService>(binder);
}

void WifiClient::binderDied(const wp<IBinder>& who)
{
    exit(-1);
}

const char *WifiClient::supStateToString(int state)
{
    static const char *sStateNames[] = {
	"Disconnected", "Disabled", "Inactive", "Scanning", "Authenticating",
	"Associating", "Associated", "WPA 4-way Handshake", "WPA Group Handshake", 
	"Completed", "Dormant", "Uninitialized", "Invalid"
    };

    if (state < 0 || state > 12)
	return "<ERROR>";
    return sStateNames[state];
}

}; // namespace Android
