/*
  Wifi State Machine

  Matches pretty closely the state machine in WifiStateMachine.java
  Please note that this class is a Thread
*/

#ifndef _WIFI_STATE_MACHINE_H
#define _WIFI_STATE_MACHINE_H

#include "StateMachine.h"
#include <utils/String8.h>
#include <wifi/IWifiClient.h>

namespace android {

class WifiMonitor;
class WifiConfigStore;
class NetworkInterface;
class ScannedStations;
class DhcpStateMachine;
class DhcpSuccessMessage;

class WifiStateMachine : public StateMachine 
{
public:
    enum {
	CMD_LOAD_DRIVER,
	CMD_UNLOAD_DRIVER,
	CMD_START_DRIVER,
	CMD_STOP_DRIVER,
	CMD_START_SUPPLICANT,
	CMD_STOP_SUPPLICANT,
	CMD_DISCONNECT,
	CMD_RECONNECT,
	CMD_REASSOCIATE,
	CMD_CONNECT_NETWORK,
	CMD_ENABLE_RSSI_POLL,
	CMD_ENABLE_BACKGROUND_SCAN,
	CMD_RSSI_POLL,
	CMD_START_SCAN,
	CMD_ADD_OR_UPDATE_NETWORK,
	CMD_REMOVE_NETWORK,
	CMD_ENABLE_NETWORK,
	CMD_DISABLE_NETWORK,
	CMD_SELECT_NETWORK,

	CMD_LOAD_DRIVER_SUCCESS,
	CMD_LOAD_DRIVER_FAILURE,
	CMD_UNLOAD_DRIVER_SUCCESS,
	CMD_UNLOAD_DRIVER_FAILURE,
	CMD_STOP_SUPPLICANT_SUCCESS,
	CMD_STOP_SUPPLICANT_FAILURE,
	
	SUP_DISCONNECTION_EVENT,
	SUP_CONNECTION_EVENT,
	SUP_SCAN_RESULTS_EVENT,
	SUP_STATE_CHANGE_EVENT,
	
	NETWORK_CONNECTION_EVENT,
	NETWORK_DISCONNECTION_EVENT,

	DHCP_SUCCESS,
	DHCP_FAILURE,

	AUTHENTICATION_FAILURE_EVENT
	// If you add to this list, fix the strings in wifistatemachine.cpp
    };

    WifiStateMachine(const char *interface);

    void startRunning();

    // These functions are used by the WifiService to extract
    // the current state for broadcast
    WifiState                 state() const;
    Vector<ScannedStation>    scandata() const;
    Vector<ConfiguredStation> configdata() const;
    WifiInformation           information() const;
    int                       rssi();
    int                       link_speed() const;

    // These functions should only be used by states
    void handleSupplicantConnection();
    int handleSupplicantStateChange(Message *);
    void handleNetworkConnect(Message *);
    void handleNetworkDisconnect();
    void handleFailedIpConfiguration();
    void handleSuccessfulIpConfiguration(const DhcpSuccessMessage *message);
    void handleScanData(const String8&);
    void handleRemoveNetwork(int);
    void handleAddOrUpdateNetwork(const ConfiguredStation&);
    void handleSelectNetwork(int);
    void handleEnableNetwork(int, bool);
    void handleDisableNetwork(int);
    void enableRssiPolling(bool);
    void enableBackgroundScan(bool);
    void fetchRssiAndLinkSpeedNative();
    void setState(WifiState state);
    void setScanResultIsPending(bool value) { mScanResultIsPending = value; }
    
    const String8&    interface() const { return mInterface; }
    WifiMonitor      *monitor() const { return mWifiMonitor; }
    WifiConfigStore  *configStore() const { return mWifiConfigStore; }
    NetworkInterface *networkInterface() const { return mNetworkInterface; }
    bool              isScanMode() const { return mIsScanMode; }
    bool              rssiPolling() const { return mEnableRssiPolling; }
    bool              backgroundScan() const { return mEnableBackgroundScan; }
    bool              scanResultIsPending() const { return mScanResultIsPending; }
    void              clearSupplicantRestartCount() { mSupplicantRestartCount = 0; }
    bool              checkSupplicantRestartCount() { return ++mSupplicantRestartCount <= 5; }

protected:
    virtual const char *msgStr(int msg_id);

private:
    String8           mInterface;
    WifiMonitor      *mWifiMonitor;
    NetworkInterface *mNetworkInterface;
    bool              mIsScanMode;
    bool              mEnableRssiPolling;
    bool              mEnableBackgroundScan;
    bool              mScanResultIsPending;
    int               mSupplicantRestartCount;

    // These elements can be accessed by an outside thread
    // so they must all be read-protected
    mutable Mutex     mReadLock; 
    WifiState         mState;
    ScannedStations  *mScannedStations;
    WifiConfigStore  *mWifiConfigStore;
    WifiInformation   mWifiInformation;   // Information about the current network
    DhcpStateMachine *mDhcpStateMachine;
};


/*
  Special message class to carry extra configuration data for network add/update
 */

class AddOrUpdateNetworkMessage : public Message {
public:
    AddOrUpdateNetworkMessage(const ConfiguredStation& cs)
	: Message(WifiStateMachine::CMD_ADD_OR_UPDATE_NETWORK)
	, mConfig(cs) {}
    const ConfiguredStation& config() const { return mConfig; }
private:
    ConfiguredStation mConfig;
};

/*
  Special message class to carry DHCP results
 */

class DhcpSuccessMessage : public Message {
public:
    DhcpSuccessMessage(const char *in_ipaddr, const char *in_gateway,
		       const char *in_dns1, const char *in_dns2, const char *in_server)
	: Message(WifiStateMachine::DHCP_SUCCESS)
	, ipaddr(in_ipaddr)
	, gateway(in_gateway)
	, dns1(in_dns1)
	, dns2(in_dns2)
	, server(in_server) {}

public:
    String8 ipaddr, gateway, dns1, dns2, server;
};


};  // namespace android

#endif // _WIFI_STATE_MACHINE_H
