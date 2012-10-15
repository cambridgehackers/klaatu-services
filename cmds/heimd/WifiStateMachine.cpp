/*
  Wifi state machine and interface.
  This object should be placed in its own thread

  This is strongly based on the WifiStateMachine.java code
  in Android.
 */

#include "HeimdDebug.h"
#include "WifiStateMachine.h"
#include "WifiConfigStore.h"
#include "WifiMonitor.h"
#include "StringUtils.h"
#include "HeimdService.h"
#include "WifiCommands.h"
#include "ScannedStations.h"
#include "SupplicantState.h"
#include "NetworkInterface.h"
#include "DhcpStateMachine.h"

#include <stdio.h>
#include <cutils/properties.h>
#include <hardware_legacy/wifi.h>

// ------------------------------------------------------------

namespace android {

const int RSSI_POLL_INTERVAL_MSECS = 3000;
const int SUPPLICANT_RESTART_INTERVAL_MSECS = 5000;


enum {
    INITIAL_STATE,                // 0
    DRIVER_LOADING_STATE,         // 1
    DRIVER_LOADED_STATE,          // 2
    DRIVER_UNLOADING_STATE,       // 3
    DRIVER_UNLOADED_STATE,        // 4
    DRIVER_FAILED_STATE,          // 5
    SUPPLICANT_STARTING_STATE,    // 6
    SUPPLICANT_STARTED_STATE,     // 7
    DRIVER_STARTED_STATE,         // 8
    DRIVER_STOPPING_STATE,        // 9
    DRIVER_STOPPED_STATE,         // 10
    SUPPLICANT_STOPPING_STATE,    // 11
    SCAN_MODE_STATE,              // 12
    DISCONNECTED_STATE,
    CONNECTING_STATE,
    CONNECTED_STATE,
    DISCONNECTING_STATE,
    CONNECT_MODE_STATE,
};

static const char *sMessageToString[] = {
    "CMD_LOAD_DRIVER",
    "CMD_UNLOAD_DRIVER",
    "CMD_START_DRIVER",
    "CMD_STOP_DRIVER",
    "CMD_START_SUPPLICANT",
    "CMD_STOP_SUPPLICANT",
    "CMD_DISCONNECT",
    "CMD_RECONNECT",
    "CMD_REASSOCIATE",
    "CMD_CONNECT_NETWORK",
    "CMD_ENABLE_RSSI_POLL",
    "CMD_ENABLE_BACKGROUND_SCAN",
    "CMD_RSSI_POLL",
    "CMD_START_SCAN",
    "CMD_ADD_OR_UPDATE_NETWORK",
    "CMD_REMOVE_NETWORK",
    "CMD_SELECT_NETWORK",
    "CMD_ENABLE_NETWORK",
    "CMD_DISABLE_NETWORK",
    
    "CMD_LOAD_DRIVER_SUCCESS",
    "CMD_LOAD_DRIVER_FAILURE",
    "CMD_UNLOAD_DRIVER_SUCCESS",
    "CMD_UNLOAD_DRIVER_FAILURE",
    "CMD_STOP_SUPPLICANT_SUCCESS",
    "CMD_STOP_SUPPLICANT_FAILURE",
    
    "SUP_DISCONNECTION_EVENT",
    "SUP_CONNECTION_EVENT",
    "SUP_SCAN_RESULTS_EVENT",
    "SUP_STATE_CHANGE_EVENT",
    
    "NETWORK_CONNECTION_EVENT",
    "NETWORK_DISCONNECTION_EVENT",

    "DHCP_SUCCESS",
    "DHCP_FAILURE",

    "AUTHENTICATION_FAILURE_EVENT"
};

// ------------------------------------------------------------

class DefaultState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_LOAD_DRIVER:
	case WifiStateMachine::CMD_UNLOAD_DRIVER:
	case WifiStateMachine::CMD_START_SUPPLICANT:
	case WifiStateMachine::CMD_STOP_SUPPLICANT:
	case WifiStateMachine::CMD_RSSI_POLL:
	case WifiStateMachine::SUP_CONNECTION_EVENT:
	case WifiStateMachine::SUP_DISCONNECTION_EVENT:
	case WifiStateMachine::CMD_START_SCAN:
	case WifiStateMachine::CMD_ADD_OR_UPDATE_NETWORK:
	case WifiStateMachine::CMD_REMOVE_NETWORK:
	case WifiStateMachine::CMD_SELECT_NETWORK:
	case WifiStateMachine::CMD_ENABLE_NETWORK:
	case WifiStateMachine::CMD_DISABLE_NETWORK:
	    break;
	case WifiStateMachine::CMD_ENABLE_RSSI_POLL:
	    wsm->enableRssiPolling(message->arg1() != 0);
	    break;
	case WifiStateMachine::CMD_ENABLE_BACKGROUND_SCAN:
	    wsm->enableBackgroundScan(message->arg1() != 0);
	    break;
	default:
	    LOGD("...ERROR - unhandled message %s (%d)\n", 
		 sMessageToString[message->command()], message->command());
	    break;
	}
	return SM_HANDLED;
    }
};


class InitialState : public State {
public:
    void enter(StateMachine *stateMachine) {
	if (::is_wifi_driver_loaded())
	    stateMachine->transitionTo(DRIVER_LOADED_STATE);
	else
	    stateMachine->transitionTo(DRIVER_UNLOADED_STATE);
    }
};

/*
  The Driver states refer to the kernel model.  Executing
  "wifi_load_driver()" causes the appropriate kernel model for your
  board to be inserted and executes a firmware loader.

  This is tied in tightly to the property system, looking at the
  "wlan.driver.status" property to see if the driver has been loaded.
 */

class DriverLoadingState : public State {
public:
    void enter(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	wsm->setState(WS_ENABLING);
	if (wifi_load_driver() == 0)
	    stateMachine->enqueue(WifiStateMachine::CMD_LOAD_DRIVER_SUCCESS);
	else {
	    wsm->setState(WS_UNKNOWN);
	    stateMachine->enqueue(WifiStateMachine::CMD_LOAD_DRIVER_FAILURE);
	}
    }

    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	switch (message->command()) {
	case WifiStateMachine::CMD_LOAD_DRIVER_SUCCESS:
	    stateMachine->transitionTo(DRIVER_LOADED_STATE);
	    return SM_HANDLED;
	case WifiStateMachine::CMD_LOAD_DRIVER_FAILURE:
	    stateMachine->transitionTo(DRIVER_FAILED_STATE);
	    return SM_HANDLED;
	case WifiStateMachine::CMD_LOAD_DRIVER:
	case WifiStateMachine::CMD_UNLOAD_DRIVER:
	case WifiStateMachine::CMD_START_SUPPLICANT:
	case WifiStateMachine::CMD_STOP_SUPPLICANT:
	case WifiStateMachine::CMD_START_DRIVER:
	case WifiStateMachine::CMD_STOP_DRIVER:
	    return SM_DEFER;
	default:
	    break;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverLoadedState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);

	switch (message->command()) {
	case WifiStateMachine::CMD_UNLOAD_DRIVER:
	    stateMachine->transitionTo(DRIVER_UNLOADING_STATE);
	    return SM_HANDLED;

	case WifiStateMachine::CMD_START_SUPPLICANT:
	    wsm->networkInterface()->wifiFirmwareReload("STA");
	    wsm->networkInterface()->setInterfaceDown();
	    if (::wifi_start_supplicant() == 0) {
		LOGV("#################### STARTING SUPPLICANT : SUCCEEDED ######################\n");
		wsm->monitor()->startRunning();
		stateMachine->transitionTo(SUPPLICANT_STARTING_STATE);
	    }
	    else {
		LOGV("#################### STARTING SUPPLICANT : FAILED ######################\n");
		stateMachine->transitionTo(DRIVER_UNLOADING_STATE);
	    }
	    return SM_HANDLED;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverUnloadingState : public State {
public:
    void enter(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	if (::wifi_unload_driver() == 0) {
	    stateMachine->enqueue(WifiStateMachine::CMD_UNLOAD_DRIVER_SUCCESS);
	    wsm->setState(WS_DISABLED);
	}
	else {
	    stateMachine->enqueue(WifiStateMachine::CMD_UNLOAD_DRIVER_FAILURE);
	    wsm->setState(WS_UNKNOWN);
	}
    }

    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	switch (message->command()) {
	case WifiStateMachine::CMD_UNLOAD_DRIVER_SUCCESS:
	    stateMachine->transitionTo(DRIVER_UNLOADED_STATE);
	    return SM_HANDLED;
	case WifiStateMachine::CMD_UNLOAD_DRIVER_FAILURE:
	    stateMachine->transitionTo(DRIVER_FAILED_STATE);
	    return SM_HANDLED;
	case WifiStateMachine::CMD_LOAD_DRIVER:
	case WifiStateMachine::CMD_UNLOAD_DRIVER:
	case WifiStateMachine::CMD_START_SUPPLICANT:
	case WifiStateMachine::CMD_STOP_SUPPLICANT:
	case WifiStateMachine::CMD_START_DRIVER:
	case WifiStateMachine::CMD_STOP_DRIVER:
	    return SM_DEFER;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverUnloadedState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	switch (message->command()) {
	case WifiStateMachine::CMD_LOAD_DRIVER:
	    stateMachine->transitionTo(DRIVER_LOADING_STATE);
	    return SM_HANDLED;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverFailedState : public State {
public:
    stateprocess_t process(StateMachine *, Message *) {
	return SM_HANDLED;
    }
};

/*
  The supplicant states ensure that the wpa_supplicant program is
  running.  This is tied in tightly to the property system, using the
  "init.svc.wpa_supplicant" property to check if it is running.
 */

class SupplicantStartingState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::SUP_CONNECTION_EVENT:
	    wsm->handleSupplicantConnection();
	    wsm->clearSupplicantRestartCount();
	    stateMachine->transitionTo(DRIVER_STARTED_STATE);
	    return SM_HANDLED;
	    
	case WifiStateMachine::SUP_DISCONNECTION_EVENT:
	    if (wsm->checkSupplicantRestartCount()) {
		LOGD("Restarting supplicant\n");
		::wifi_stop_supplicant();
		stateMachine->enqueueDelayed(WifiStateMachine::CMD_START_SUPPLICANT, 
					     SUPPLICANT_RESTART_INTERVAL_MSECS);
	    } else {
		LOGD("Failed to start supplicant; unloading driver\n");
		wsm->clearSupplicantRestartCount();
		stateMachine->enqueue(WifiStateMachine::CMD_UNLOAD_DRIVER);
	    }
	    stateMachine->transitionTo(DRIVER_LOADED_STATE);
	    return SM_HANDLED;

	case WifiStateMachine::CMD_LOAD_DRIVER:
	case WifiStateMachine::CMD_UNLOAD_DRIVER:
	case WifiStateMachine::CMD_START_SUPPLICANT:
	case WifiStateMachine::CMD_STOP_SUPPLICANT:
	case WifiStateMachine::CMD_START_DRIVER:
	case WifiStateMachine::CMD_STOP_DRIVER:
	    return SM_DEFER;
	}
	return SM_NOT_HANDLED;
    }
};

/*
  This superclass state encapsulates all of:
     DriverStarting, DriverStopping, DriverStarted, DriverStarted
     ScanMode
     ConnectMode
     Connecting, Disconnected, Connected, Disconnecting
 */

class SupplicantStartedState : public State {
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_STOP_SUPPLICANT:
	    stateMachine->transitionTo(SUPPLICANT_STOPPING_STATE);
	    return SM_HANDLED;
	case WifiStateMachine::SUP_DISCONNECTION_EVENT:
	    LOGD("Connection lost, restarting supplicant\n");
	    ::wifi_stop_supplicant();
	    ::wifi_close_supplicant_connection();
	    // mNetworkInfo.setIsAvailable(false);
	    wsm->handleNetworkDisconnect();
	    // sendSupplicantConnectionChangedBroadcast(false);
	    // mSupplicantStateTracker.sendMessage(CMD_RESET_SUPPLICANT_STATE);
	    // mWpsStateMachine.sendMessage(CMD_RESET_WPS_STATE);
	    stateMachine->transitionTo(DRIVER_LOADED_STATE);
	    stateMachine->enqueueDelayed(WifiStateMachine::CMD_START_SUPPLICANT, 
					 SUPPLICANT_RESTART_INTERVAL_MSECS);
	    return SM_HANDLED;
	case WifiStateMachine::SUP_SCAN_RESULTS_EVENT: {
	    LOGV("...extracting scan results from supplicant\n");
	    String8 networks = doWifiStringCommand("SCAN_RESULTS");
	    wsm->handleScanData(networks);
	} return SM_HANDLED;
	case WifiStateMachine::CMD_ADD_OR_UPDATE_NETWORK: {
	    AddOrUpdateNetworkMessage *msg = static_cast<AddOrUpdateNetworkMessage *>(message);
	    wsm->handleAddOrUpdateNetwork(msg->config());
	    return SM_HANDLED;
	} break;
	case WifiStateMachine::CMD_REMOVE_NETWORK:
	    wsm->handleRemoveNetwork(message->arg1());
	    return SM_HANDLED;
	case WifiStateMachine::CMD_SELECT_NETWORK:
	    wsm->handleSelectNetwork(message->arg1());
	    return SM_HANDLED;
	case WifiStateMachine::CMD_ENABLE_NETWORK:
	    wsm->handleEnableNetwork(message->arg1(), message->arg2() != 0);
	    return SM_HANDLED;
	case WifiStateMachine::CMD_DISABLE_NETWORK:
	    wsm->handleDisableNetwork(message->arg1());
	    return SM_HANDLED;
	}
	return SM_NOT_HANDLED;
    }

    void exit(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	wsm->handleNetworkDisconnect();
    }
};

class SupplicantStoppingState : public State {
public:
    void enter(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	wsm->setState(WS_DISABLING);
	LOGD("########### Asking supplicant to terminate\n");
	if (!doWifiBooleanCommand("OK", "TERMINATE")) {
	    LOGW("########## Supplicant not stopping nicely; killing\n");
	    ::wifi_stop_supplicant();
	}
	stateMachine->transitionTo(DRIVER_LOADED_STATE);
    }
};


/*
  The Driver Start/Stop states are all about controlling if the
  interface is running or not (wpa_state=INTERFACE_DISABLED)

  This is changed with the wpa_cli command "driver start" and "driver
  stop"
 */

class DriverStartedState : public State {
public:
    void enter(StateMachine *stateMachine) {
	LOGV(".........driver started state enter....\n");
	// Set country code if available
	// setFrequencyBand();
//	setNetworkDetailedState(DISCONNECTED);
	if (static_cast<WifiStateMachine *>(stateMachine)->isScanMode()) {
	    doWifiBooleanCommand("OK", "AP_SCAN 2");  // SCAN_ONLY_MODE
	    doWifiBooleanCommand("OK", "DISCONNECT");
	    stateMachine->transitionTo(SCAN_MODE_STATE);
	} else {
	    doWifiBooleanCommand("OK", "AP_SCAN 1");  // CONNECT_MODE
	    doWifiBooleanCommand("OK", "RECONNECT");
	    stateMachine->transitionTo(DISCONNECTED_STATE);
	}
    }
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_START_SCAN: {
	    bool force_active = (message->arg1() != 0);
	    if (force_active) 
		doWifiBooleanCommand("OK", "DRIVER SCAN-ACTIVE");
	    doWifiBooleanCommand("OK", "SCAN");
	    if (force_active)
		doWifiBooleanCommand("OK", "DRIVER SCAN-PASSIVE");
	    wsm->setScanResultIsPending(true);
	    return SM_HANDLED;
	} break;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverStoppingState : public State {
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::SUP_STATE_CHANGE_EVENT: {
	    int state = wsm->handleSupplicantStateChange(message);
	    if (state == SupplicantState::INTERFACE_DISABLED)
		stateMachine->transitionTo(DRIVER_STOPPED_STATE);
	}  return SM_HANDLED;

	case WifiStateMachine::CMD_START_DRIVER:
	case WifiStateMachine::CMD_STOP_DRIVER:
	case WifiStateMachine::NETWORK_CONNECTION_EVENT:
	case WifiStateMachine::NETWORK_DISCONNECTION_EVENT:
	case WifiStateMachine::AUTHENTICATION_FAILURE_EVENT:
	case WifiStateMachine::CMD_START_SCAN:
	case WifiStateMachine::CMD_DISCONNECT:
	case WifiStateMachine::CMD_REASSOCIATE:
	case WifiStateMachine::CMD_RECONNECT:
	    return SM_DEFER;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverStoppedState : public State {
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_START_DRIVER:
	    // TODO: Insert WifiNative.startDriverCommand()
	    // transitionTo(DRIVER_STARTING_STATE);
	    break;

	case WifiStateMachine::CMD_STOP_DRIVER:
	case WifiStateMachine::NETWORK_CONNECTION_EVENT:
	case WifiStateMachine::NETWORK_DISCONNECTION_EVENT:
	case WifiStateMachine::AUTHENTICATION_FAILURE_EVENT:
	case WifiStateMachine::CMD_DISCONNECT:
	case WifiStateMachine::CMD_REASSOCIATE:
	case WifiStateMachine::CMD_RECONNECT:
	    return SM_DEFER;
	}
	return SM_NOT_HANDLED;
    }
};

class DriverStartingState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::SUP_STATE_CHANGE_EVENT:
	    wsm->handleSupplicantStateChange(message);
	    return SM_HANDLED;

	case WifiStateMachine::CMD_START_DRIVER:
	case WifiStateMachine::CMD_STOP_DRIVER:
	case WifiStateMachine::NETWORK_CONNECTION_EVENT:
	case WifiStateMachine::NETWORK_DISCONNECTION_EVENT:
	case WifiStateMachine::AUTHENTICATION_FAILURE_EVENT:
	case WifiStateMachine::CMD_START_SCAN:
	case WifiStateMachine::CMD_DISCONNECT:
	case WifiStateMachine::CMD_REASSOCIATE:
	case WifiStateMachine::CMD_RECONNECT:
	    return SM_DEFER;
	}
	return SM_NOT_HANDLED;
    }
};

/*
  The ConnectMode and ScanModes are the 'normal' range of the Wifi
  driver.  In these modes, we know that the kernel modules have been
  loaded, that wpa_supplicant is currently running, and that the
  wpa_supplicant driver has been started.

  We cycle between being connected, disconnected, or in "scan" mode.
 */

class ScanModeState : public State {
};

// CONNECT_MODE = 1
// SCAN_ONLY_MODE = 2

class ConnectModeState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::AUTHENTICATION_FAILURE_EVENT:
	    LOGV("TODO: Authentication failure\n");
	    return SM_HANDLED;
	case WifiStateMachine::SUP_STATE_CHANGE_EVENT:
	    LOGV("...ConnectMode: Supplicant state change event\n");
	    wsm->handleSupplicantStateChange(message);
	    return SM_HANDLED;
	case WifiStateMachine::CMD_DISCONNECT:
	    doWifiBooleanCommand("OK", "DISCONNECT");
	    return SM_HANDLED;
	case WifiStateMachine::CMD_RECONNECT:
	    doWifiBooleanCommand("OK", "RECONNECT");
	    return SM_HANDLED;
	case WifiStateMachine::CMD_REASSOCIATE:
	    doWifiBooleanCommand("OK", "REASSOCIATE");
	    return SM_HANDLED;
	case WifiStateMachine::CMD_CONNECT_NETWORK: {
	    int network_id = message->arg1();
	    LOGV("TODO: WifiConfigStore.selectNetwork(network_id=%d);\n", network_id);
	} return SM_HANDLED;
	case WifiStateMachine::SUP_SCAN_RESULTS_EVENT:
	    // Go back to "connect" mode
	    LOGV(".....Switching back to CONNECT_MODE\n");
	    doWifiBooleanCommand("OK", "AP_SCAN 1");  // CONNECT_MODE
	    return SM_NOT_HANDLED;
	case WifiStateMachine::NETWORK_CONNECTION_EVENT: {
	    wsm->handleNetworkConnect(message);
	    stateMachine->transitionTo(CONNECTING_STATE);
	} return SM_HANDLED;
	case WifiStateMachine::NETWORK_DISCONNECTION_EVENT:
	    wsm->handleNetworkDisconnect();
	    stateMachine->transitionTo(DISCONNECTED_STATE);
	    return SM_HANDLED;
	}

	return SM_NOT_HANDLED;
    }
};

class ConnectingState : public State {
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_START_SCAN:
	    return SM_DEFER;

	case WifiStateMachine::DHCP_SUCCESS:
	    wsm->handleSuccessfulIpConfiguration(static_cast<DhcpSuccessMessage *>(message));
	    stateMachine->transitionTo(CONNECTED_STATE);
	    return SM_HANDLED;

	case WifiStateMachine::DHCP_FAILURE:
	    wsm->handleFailedIpConfiguration();
	    stateMachine->transitionTo(DISCONNECTING_STATE);
	    return SM_HANDLED;
	}
	return SM_NOT_HANDLED;
    }
};

class ConnectedState : public State {
public:
    void enter(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	if (wsm->rssiPolling())
	    stateMachine->enqueueDelayed(WifiStateMachine::CMD_RSSI_POLL, RSSI_POLL_INTERVAL_MSECS);
    }

    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_DISCONNECT:
	    doWifiBooleanCommand("OK", "DISCONNECT");
	    stateMachine->transitionTo(DISCONNECTING_STATE);
	    return SM_HANDLED;

	case WifiStateMachine::CMD_START_SCAN:
	    // Put the network briefly into scan-only mode
	    doWifiBooleanCommand("OK", "AP_SCAN 2");   // SCAN_ONLY_MODE
	    return SM_NOT_HANDLED;

	case WifiStateMachine::CMD_RSSI_POLL:
	    if (wsm->rssiPolling()) {
		wsm->fetchRssiAndLinkSpeedNative();
		stateMachine->enqueueDelayed(WifiStateMachine::CMD_RSSI_POLL, RSSI_POLL_INTERVAL_MSECS);
	    }
	    return SM_HANDLED;

	case WifiStateMachine::CMD_ENABLE_RSSI_POLL:
	    wsm->enableRssiPolling(message->arg1() != 0);
	    if (wsm->rssiPolling()) {
		wsm->fetchRssiAndLinkSpeedNative();
		stateMachine->enqueueDelayed(WifiStateMachine::CMD_RSSI_POLL, RSSI_POLL_INTERVAL_MSECS);
	    }
	    return SM_HANDLED;
	}
	return SM_NOT_HANDLED;
    }

    void exit(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	if (wsm->scanResultIsPending())
	    doWifiBooleanCommand("OK", "AP_SCAN 1");  // CONNECT_MODE
    }
};

class DisconnectingState : public State {
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::SUP_STATE_CHANGE_EVENT:
	    wsm->handleNetworkDisconnect();
	    stateMachine->transitionTo(DISCONNECTED_STATE);
	    return SM_DEFER;
	}
	return SM_NOT_HANDLED;
    }
};

class DisconnectedState : public State {
    void enter(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	if (wsm->backgroundScan()) {
	    if (!wsm->scanResultIsPending())
		doWifiBooleanCommand("OK", "DRIVER BGSCAN-START");
	}
    }

    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	switch (message->command()) {
	case WifiStateMachine::CMD_START_SCAN:
	    /* Disable background scan temporarily during a regular scan */
	    if (wsm->backgroundScan())
		doWifiBooleanCommand("OK", "DRIVER BGSCAN-STOP");
	    return SM_NOT_HANDLED;  // Handle in parent state

	case WifiStateMachine::CMD_ENABLE_BACKGROUND_SCAN:
	    wsm->enableBackgroundScan(message->arg1() != 0);
	    if (wsm->backgroundScan())
		doWifiBooleanCommand("OK", "DRIVER BGSCAN-START");
	    else
		doWifiBooleanCommand("OK", "DRIVER BGSCAN-STOP");
	    return SM_HANDLED;

	case WifiStateMachine::SUP_SCAN_RESULTS_EVENT:
	    /* Re-enable background scan when a pending scan result is received */
	    if (wsm->backgroundScan() && wsm->scanResultIsPending())
		doWifiBooleanCommand("OK", "DRIVER BGSCAN-START");
	    return SM_NOT_HANDLED;  // Handle in parent state
	}
	return SM_NOT_HANDLED;
    }

    void exit(StateMachine *stateMachine) {
	WifiStateMachine *wsm = static_cast<WifiStateMachine *>(stateMachine);
	if (wsm->backgroundScan())
	    doWifiBooleanCommand("OK", "DRIVER BGSCAN-STOP");
    }
};


// ------------------------------------------------------------

WifiStateMachine::WifiStateMachine(const char *interface) 
    : mInterface(interface)
    , mIsScanMode(false)
    , mEnableRssiPolling(true)
    , mEnableBackgroundScan(false)
    , mScanResultIsPending(false)
    , mState(WS_DISABLED)
{
    mWifiMonitor      = new WifiMonitor(this);
    mWifiConfigStore  = new WifiConfigStore;
    mNetworkInterface = new NetworkInterface(this, mInterface);
    mScannedStations  = new ScannedStations;
    mDhcpStateMachine = new DhcpStateMachine(this, mInterface);

    // Ensure we've terminated DHCP and the wpa_supplicant
    // The DHCP state machine ensures that the old dhcp is stopped
    LOGV("Stopping supplicant, result=%d\n", ::wifi_stop_supplicant());

    DefaultState *default_state = new DefaultState;
    DriverUnloadedState *unloaded_state = new DriverUnloadedState;
    DriverStartedState *driver_started = new DriverStartedState;
    SupplicantStartedState *sup_started_state = new SupplicantStartedState;
    ConnectModeState *connect_mode_state = new ConnectModeState;

    add(INITIAL_STATE, "Initial", new InitialState, default_state);
    add(DRIVER_LOADING_STATE, "DriverLoading", new DriverLoadingState, default_state);
    add(DRIVER_LOADED_STATE, "DriverLoaded", new DriverLoadedState, default_state);
    add(DRIVER_UNLOADING_STATE, "DriverUnloading", new DriverUnloadingState, default_state);
    add(DRIVER_UNLOADED_STATE, "DriverUnloaded", unloaded_state, default_state);
      add(DRIVER_FAILED_STATE, "DriverFailed", new DriverFailedState, unloaded_state);

    add(SUPPLICANT_STARTING_STATE, "SupplicantStarting", new SupplicantStartingState, default_state);
    add(SUPPLICANT_STOPPING_STATE, "SupplicantStopping", new SupplicantStoppingState, default_state);
    add(SUPPLICANT_STARTED_STATE, "SupplicantStarted", sup_started_state, default_state);
      add(DRIVER_STOPPING_STATE, "DriverStopping", new DriverStoppingState, sup_started_state);
      add(DRIVER_STOPPED_STATE, "DriverStopped", new DriverStoppedState, sup_started_state);
      add(DRIVER_STARTED_STATE, "DriverStarted", driver_started, sup_started_state);
        add(SCAN_MODE_STATE, "ScanMode", new ScanModeState, driver_started);
        add(CONNECT_MODE_STATE, "ConnectMode", connect_mode_state, driver_started);
          add(CONNECTING_STATE, "Connecting", new ConnectingState, connect_mode_state);
	  add(CONNECTED_STATE, "Connected", new ConnectedState, connect_mode_state);
	  add(DISCONNECTED_STATE, "Disconnected", new DisconnectedState, connect_mode_state);
	  add(DISCONNECTING_STATE, "Disconnecting", new DisconnectingState, connect_mode_state);
    transitionTo(INITIAL_STATE);
}

/* Begin functions called from a heimd thread */

void WifiStateMachine::startRunning()
{
    mNetworkInterface->startRunning();

    LOGV("...................WifiStateMachine::startRunning()\n");
    status_t result = run("WifiStateMachine", PRIORITY_NORMAL);
    LOG_ALWAYS_FATAL_IF(result, "Could not start WifiStateMachine thread due to error %d\n", result);
}

WifiState WifiStateMachine::state() const
{
    Mutex::Autolock _l(mReadLock);
    return mState;
}

Vector<ScannedStation> WifiStateMachine::scandata() const
{
    Mutex::Autolock _l(mReadLock);
    return mScannedStations->data();
}

Vector<ConfiguredStation> WifiStateMachine::configdata() const
{
    Mutex::Autolock _l(mReadLock);
    return mWifiConfigStore->stationList();
}

WifiInformation WifiStateMachine::information() const
{
    Mutex::Autolock _l(mReadLock);
    return mWifiInformation;
}

int WifiStateMachine::rssi()
{
    Mutex::Autolock _l(mReadLock);
    return mWifiInformation.rssi;
}

int WifiStateMachine::link_speed() const
{
    Mutex::Autolock _l(mReadLock);
    return mWifiInformation.link_speed;
}

/* End functions called from a heimd thread */

void WifiStateMachine::handleSupplicantConnection()
{
    setState(WS_ENABLED);

    // Returns data = 'Macaddr = XX:XX:XX:XX:XX:XX'
    String8 data = doWifiStringCommand("DRIVER MACADDR");
    if (strncmp("Macaddr = ", data.string(), 10)) {
	LOGW("Unable to retrieve MAC address in wireless driver\n");
    } else {
	Mutex::Autolock _l(mReadLock);
	mWifiInformation.macaddr = String8(data.string() + 10);
	HeimdService::getInstance().BroadcastInformation(mWifiInformation);
    }
    Mutex::Autolock _l(mReadLock);
    // ### TODO: This can stall.  Not a good idea
    mWifiConfigStore->initialize();
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

int WifiStateMachine::handleSupplicantStateChange(Message *message)
{
//    LOGV("....TODO: handleSupplicantStatechange\n");
    int network_id = message->arg1();
    int sup_state  = message->arg2();
    String8 bssid = message->string();

    Mutex::Autolock _l(mReadLock);
    mWifiInformation.supplicant_state = sup_state;
    if (SupplicantState::isConnecting(sup_state)) 
	mWifiInformation.network_id = network_id;
    else
	mWifiInformation.network_id = SupplicantState::INVALID;
    
    if (sup_state == SupplicantState::ASSOCIATING)
	mWifiInformation.bssid = bssid;

    HeimdService::getInstance().BroadcastInformation(mWifiInformation);
    return sup_state;
}

void WifiStateMachine::handleNetworkConnect(Message *message)
{
    LOGV("....handleNetworkConnect(%p)\n", message);
    mDhcpStateMachine->enqueue(DhcpStateMachine::CMD_START_DHCP);

    int network_id = message->arg1();
    String8 bssid = message->string();

    Mutex::Autolock _l(mReadLock);
    mWifiInformation.bssid = bssid;
    mWifiInformation.network_id = network_id;
    String8 status = doWifiStringCommand("STATUS");
    LOGV("....checking status '%s'\n", status.string());
    Vector<String8> lines = splitString(status, '\n');
    for (size_t i=0 ; i < lines.size() ; i++) {
	Vector<String8> pair = splitString(lines[i], '=');
	if (pair.size() == 2 && pair[0] == "ssid")
	    mWifiInformation.ssid = pair[1];
    }
    HeimdService::getInstance().BroadcastInformation(mWifiInformation);

    mWifiConfigStore->networkConnect(network_id);
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

void WifiStateMachine::handleNetworkDisconnect()
{
    LOGV("....handleNetworkDisconnect(): Stop DHCP and clear IP\n");
    mDhcpStateMachine->enqueue(DhcpStateMachine::CMD_STOP_DHCP);
    mNetworkInterface->clearInterfaceAddresses();

    // Update the Wifi Information visible to the user
    Mutex::Autolock _l(mReadLock);
    mWifiInformation.ipaddr = "";
    mWifiInformation.bssid = "";
    mWifiInformation.ssid = "";
    mWifiInformation.network_id = -1;
    mWifiInformation.supplicant_state = SupplicantState::UNINITIALIZED;
    mWifiInformation.rssi = -9999;
    mWifiInformation.link_speed = -1;
    HeimdService::getInstance().BroadcastInformation(mWifiInformation);

    mWifiConfigStore->networkDisconnect();
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

void WifiStateMachine::handleFailedIpConfiguration()
{
}

static bool fixDnsEntry(const char *key, const char *value)
{
    char old[PROPERTY_VALUE_MAX];
    property_get(key, old, "");
    if (strcmp(old, value) != 0) {
	property_set(key, value);
	return true;
    }
    return false;
}

void WifiStateMachine::handleSuccessfulIpConfiguration(const DhcpSuccessMessage *message)
{
    LOGV("Got DHCP ipaddr=%s gateway=%s dns1=%s dns2=%s server=%s\n", 
	 message->ipaddr.string(), message->gateway.string(), message->dns1.string(), 
	 message->dns2.string(), message->server.string());
    // Update property system with DNS data for the resolver
    if (fixDnsEntry("net.dns1", message->dns1.string()) 
	|| fixDnsEntry("net.dns2", message->dns2.string())) {
	// ### TODO: Check to make sure they aren't local addresses
	mNetworkInterface->setDns(message->dns1.string(), 
				  message->dns2.string());
    }
    Mutex::Autolock _l(mReadLock);
    mWifiInformation.ipaddr = message->ipaddr;
    HeimdService::getInstance().BroadcastInformation(mWifiInformation);
}

void WifiStateMachine::handleScanData(const String8& data)
{
    Mutex::Autolock _l(mReadLock);
    mScannedStations->update(data);
    mScanResultIsPending = false;
    HeimdService::getInstance().BroadcastScanResults(mScannedStations->data());
}

/*
  This is actually the "forgetNetwork" command from WifiConfigStore
 */

void WifiStateMachine::handleRemoveNetwork(int network_id) 
{
    LOGV("REMOVING NETWORK %d\n", network_id);
    Mutex::Autolock _l(mReadLock);
    mWifiConfigStore->removeNetwork(network_id);
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}    

void WifiStateMachine::handleAddOrUpdateNetwork(const ConfiguredStation& config)
{
    Mutex::Autolock _l(mReadLock);
    int network_id = mWifiConfigStore->addOrUpdate(config);
    if (network_id != -1)
	mWifiConfigStore->selectNetwork(network_id);
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

void WifiStateMachine::handleSelectNetwork(int network_id)
{
    Mutex::Autolock _l(mReadLock);
    mWifiConfigStore->selectNetwork(network_id);
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

void WifiStateMachine::handleEnableNetwork(int network_id, bool disable_others)
{
    Mutex::Autolock _l(mReadLock);
    mWifiConfigStore->enableNetwork(network_id, disable_others);
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

void WifiStateMachine::handleDisableNetwork(int network_id)
{
    Mutex::Autolock _l(mReadLock);
    mWifiConfigStore->disableNetwork(network_id);
    HeimdService::getInstance().BroadcastConfiguredStations(mWifiConfigStore->stationList());
}

void WifiStateMachine::enableRssiPolling(bool enable)
{
    mEnableRssiPolling = enable;
}

void WifiStateMachine::enableBackgroundScan(bool enable)
{
    mEnableBackgroundScan = enable;
}

void WifiStateMachine::fetchRssiAndLinkSpeedNative()
{
    String8 poll = doWifiStringCommand("SIGNAL_POLL");
    Vector<String8> elements = splitString(poll, '\n');
    int rssi = -1;
    int link_speed = -1;
    for (size_t i = 0 ; i < elements.size() ; i++) {
	Vector<String8> pair = splitString(elements[i], '=');
	if (pair.size() == 2) {
	    if (pair[0] == "RSSI")
		rssi = atoi(pair[1].string());
	    else if (pair[0] == "LINKSPEED") 
		link_speed = atoi(pair[1].string());
	}
    }

    Mutex::Autolock _l(mReadLock);
    mWifiInformation.rssi = rssi != -1 ? rssi : -9999;
    HeimdService::getInstance().BroadcastRssi(mWifiInformation.rssi);

    if (link_speed != -1) {
	mWifiInformation.link_speed = link_speed;
	HeimdService::getInstance().BroadcastLinkSpeed(link_speed);
    }

    HeimdService::getInstance().BroadcastInformation(mWifiInformation);
}

void WifiStateMachine::setState(WifiState state)
{
    Mutex::Autolock _l(mReadLock);
    if (state != mState) {
	mState = state;
	HeimdService::getInstance().BroadcastState(state);
    }
}

const char * WifiStateMachine::msgStr(int msg_id)
{
    return sMessageToString[msg_id];
}

};  // namespace android

