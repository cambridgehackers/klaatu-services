/*
  The WifiService is the binder object exposed by Wifi.  
 */

#ifndef _WIFI_SERVICE_H
#define _WIFI_SERVICE_H

#include <binder/BinderService.h>
#include <wifi/IWifiService.h>
#include <utils/List.h>

namespace android {
class WifiServerClient;
class WifiStateMachine;
class WifiService : // public BinderService<WifiService>,
	public BnWifiService,
	public IBinder::DeathRecipient
{
public:
    WifiService();
    virtual ~WifiService() {}

    // IBinder::DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);

    // BnWifiService
    virtual void Register(const sp<IWifiClient>& client, WifiClientFlag flags);
    virtual void SetEnabled(bool enabled);
    virtual void SendCommand(int command, int arg1, int arg2);
    virtual void AddOrUpdateNetwork(const ConfiguredStation& cs);

    // Functions invoked by the WifiStateMachine
    void BroadcastState(WifiState state);
    void BroadcastScanResults(const Vector<ScannedStation>& scandata);
    void BroadcastConfiguredStations(const Vector<ConfiguredStation>& configdata);
    void BroadcastInformation(const WifiInformation& info);
    void BroadcastRssi(int rssi);
    void BroadcastLinkSpeed(int link_speed);

private:
    mutable Mutex     mLock;
    WifiStateMachine *mWifiStateMachine;
    KeyedVector< wp<IBinder>, WifiServerClient *> mClients;
    WifiState         mState;
 };
}; // namespace android

#endif // _WIFI_SERVICE_H
