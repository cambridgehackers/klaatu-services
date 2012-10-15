/*
 The WifiConfigStore contains information about the current 
 WPA configuration.  This is copied pretty directly from WifiConfigStore.java
 */

#ifndef _WIFI_CONFIG_STORE_H
#define _WIFI_CONFIG_STORE_H

#include <heimd/IHeimdClient.h>
#include <binder/Parcel.h>

namespace android {

class WifiConfigStore {
public:
    WifiConfigStore() {}

    void initialize();
    void loadConfiguredNetworks();
    void enableAllNetworks();

    void networkConnect(int network_id);
    void networkDisconnect();

    void removeNetwork(int network_id);
    int  addOrUpdate(const ConfiguredStation& cs);

    void selectNetwork(int network_id);
    void enableNetwork(int network_id, bool disable_others=false);
    void disableNetwork(int network_id);

    Vector<ConfiguredStation> stationList() const { return mStations; }

private:
    int findIndexByNetworkId(int network_id);
    int findIndexBySsid(const String8& ssid);

private:
    Vector<ConfiguredStation>  mStations;
};

};  // namespace android

#endif // _WIFI_CONFIG_STORE_H
