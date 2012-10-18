/*
  Common client interface for talking with Wifi
*/

#ifndef _WIFI_CLIENT_H
#define _WIFI_CLIENT_H

#include <wifi/IWifiClient.h>
#include <wifi/IWifiService.h>

#include <utils/List.h>
#include <utils/Vector.h>
#include <binder/Parcel.h>

namespace android {


/**
 * Interface back to a client window
 *
 * Your client code should inherit from this interface and override the
 * functions that it wants to handle.
 */

class WifiClient : public BnWifiClient, public IBinder::DeathRecipient
{
public:
    // Request actions on the server
    void Register(WifiClientFlag flags);
    void SetEnabled(bool enable);

    void StartScan(bool force_active);
    void EnableRssiPolling(bool enable);
    void EnableBackgroundScan(bool enable);

    void AddOrUpdateNetwork(const ConfiguredStation&);
    void RemoveNetwork(int network_id);
    void SelectNetwork(int network_id);
    void EnableNetwork(int network_id, bool disable_others);
    void DisableNetwork(int network_id);
    void Reconnect();
    void Disconnect();
    void Reassociate();

    // BnWifiClient
    // The default implementations do nothing; override them in your client
    virtual void State(WifiState state);
    virtual void ScanResults(const Vector<ScannedStation>& scandata);
    virtual void ConfiguredStations(const Vector<ConfiguredStation>& configdata);
    virtual void Information(const WifiInformation& info);
    virtual void Rssi(int rssi);
    virtual void LinkSpeed(int link_speed);

    static const char *supStateToString(int state);

private:
    virtual void onFirstRef();
    virtual void binderDied(const wp<IBinder>&);

private:
    sp<IWifiService> mWifiService;
};

};

#endif  // _WIFI_CLIENT_H
