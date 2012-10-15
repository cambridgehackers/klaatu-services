/*
  The HeimdService is the binder object exposed by Heimd.  
 */

#ifndef _HEIMD_SERVICE_H
#define _HEIMD_SERVICE_H

//#include <binder/BinderService.h>
#include <heimd/IHeimdService.h>
#include <utils/Singleton.h>
#include <utils/List.h>

#include "WifiStateMachine.h"

namespace android {

class HeimdServerClient;

class HeimdService : // public BinderService<HeimdService>,
	public BnHeimdService,
	public IBinder::DeathRecipient,
	public Singleton<HeimdService>
{
//    friend class BinderService<HeimdService>;

public:
    // for HeimdService
    static const char *getServiceName() { return "heimd"; }

    HeimdService();
    virtual ~HeimdService();

    // IBinder::DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);

    // BnHeimdService
    virtual void Register(const sp<IHeimdClient>& client, HeimdClientFlag flags);
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
    HeimdServerClient * getClientLocked(const sp<IHeimdClient>&);

private:
    mutable Mutex     mLock;
    WifiStateMachine *mWifiStateMachine;
    KeyedVector< wp<IBinder>, HeimdServerClient *> mClients;
 };

// ---------------------------------------------------------------------------

}; // namespace android


#endif // _HEIMD_SERVICE_H

