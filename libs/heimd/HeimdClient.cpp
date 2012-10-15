/*
 */

#include <heimd/HeimdClient.h>
#include <binder/BinderService.h>

namespace android {

void HeimdClient::Register(HeimdClientFlag flags)
{
    mHeimdService->Register(this, flags);
}

void HeimdClient::SetEnabled(bool enable)
{
    mHeimdService->SetEnabled(enable);
}

void HeimdClient::StartScan(bool force_active)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_START_SCAN, force_active, 0);
}

void HeimdClient::EnableRssiPolling(bool enable)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_ENABLE_RSSI_POLLING, enable, 0);
}

void HeimdClient::EnableBackgroundScan(bool enable)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_ENABLE_BACKGROUND_SCAN, enable, 0);
}

void HeimdClient::AddOrUpdateNetwork(const ConfiguredStation& cs)
{
    mHeimdService->AddOrUpdateNetwork(cs);
}

void HeimdClient::RemoveNetwork(int network_id)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_REMOVE_NETWORK, network_id, 0);
}

void HeimdClient::SelectNetwork(int network_id)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_SELECT_NETWORK, network_id, 0);
}

void HeimdClient::EnableNetwork(int network_id, bool disable_others)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_ENABLE_NETWORK, 
			       network_id, disable_others);
}

void HeimdClient::DisableNetwork(int network_id)
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_DISABLE_NETWORK, network_id, 0);
}

void HeimdClient::Reconnect()
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_RECONNECT, 0, 0);
}

void HeimdClient::Disconnect()
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_DISCONNECT, 0, 0);
}

void HeimdClient::Reassociate()
{
    mHeimdService->SendCommand(IHeimdService::COMMAND_REASSOCIATE, 0, 0);
}

void HeimdClient::onFirstRef()
{
    sp<IServiceManager> sm     = defaultServiceManager();
    sp<IBinder>         binder = sm->getService(String16("heimd"));

    if (!binder.get()) {
	fprintf(stderr, "Unable to connect to heimd service\n");
	exit(-1);
    }
    status_t err = binder->linkToDeath(this, 0);
    if (err) {
	fprintf(stderr, "Unable to link to heimd death: %s", strerror(-err));
	exit(-1);
    }
 
    mHeimdService = interface_cast<IHeimdService>(binder);
}

void HeimdClient::binderDied(const wp<IBinder>& who)
{
    exit(-1);
}

void HeimdClient::State(WifiState state)
{
}

void HeimdClient::ScanResults(const Vector<ScannedStation>& scandata)
{
}

void HeimdClient::ConfiguredStations(const Vector<ConfiguredStation>& configdata)
{
}

void HeimdClient::Information(const WifiInformation& info)
{
}

void HeimdClient::Rssi(int rssi)
{
}

void HeimdClient::LinkSpeed(int link_speed)
{
}

const char *HeimdClient::supStateToString(int state)
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
