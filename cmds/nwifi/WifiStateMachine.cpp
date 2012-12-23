/*
  Wifi state machine and interface.
  This object should be placed in its own thread

  This is strongly based on the WifiStateMachine.java code
  in Android.
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <cutils/properties.h>
#include <hardware_legacy/wifi.h>
#include <netutils/dhcp.h>
#include <utils/String8.h>

#include "WifiDebug.h"
#define FSM_ACTION_CODE
#include "WifiStateMachine.h"
#include "StringUtils.h"
#include "WifiService.h"
#include "NetworkInterface.h"

namespace android {

static const int BUF_SIZE=256;
static const int RSSI_POLL_INTERVAL_MSECS = 3000;
static const int SUPPLICANT_RESTART_INTERVAL_MSECS = 5000;
enum { WIFI_LOAD_DRIVER = 1, WIFI_UNLOAD_DRIVER, WIFI_IS_DRIVER_LOADED,
    WIFI_START_SUPPLICANT, WIFI_STOP_SUPPLICANT,
    WIFI_CONNECT_SUPPLICANT, WIFI_CLOSE_SUPPLICANT, WIFI_WAIT_EVENT};

class SupplicantState {
/* Supplicant State Matches pretty closely with SupplicantState.java */
public:
    // These enumerations match the order defined in SupplicantState.java
    // which eventually match "defs.h" from wpa_supplicant
    // But not really very well - it seems that Android has been messing around...
    // with the order.  My guess is that this will change in the future.
    enum { 
	   DISCONNECTED = 0,  // Copied from "defs.h" in wpa_supplicant.c
	   INTERFACE_DISABLED,   // 1. The network interface is disabled
	   INACTIVE,             // 2. No enabled networks in the configuration
	   SCANNING,             // 3. Scanning for a network
	   AUTHENTICATING,       // 4. Driver authentication with the BSS
	   ASSOCIATING,          // 5. Driver associating with BSS (ap_scan=1)
	   ASSOCIATED,           // 6. Association successfully completed
	   FOUR_WAY_HANDSHAKE,   // 7. WPA 4-way key handshake has started
	   GROUP_HANDSHAKE,      // 8. WPA 4-way key completed; group rekeying started
	   COMPLETED,            // 9. All authentication is complete.  Now DHCP!
	   DORMANT,              // 10. Android-specific state when client issues DISCONNECT
	   UNINITIALIZED,        // 11. Android-specific state where wpa_supplicant not running
	   INVALID               // 12. Pseudo-state; should not be seen
    };
    static bool isConnecting(int state) {
	switch (state) {
	case AUTHENTICATING:
	case ASSOCIATING:
	case ASSOCIATED:
	case FOUR_WAY_HANDSHAKE:
	case GROUP_HANDSHAKE:
	case COMPLETED:
	    return true;
	}
	return false;
    }
};

int WifiStateMachine::request_wifi(int request)
{
    static const char *reqname[] = {"",
        "WIFI_LOAD_DRIVER", "WIFI_UNLOAD_DRIVER", "WIFI_IS_DRIVER_LOADED",
        "WIFI_START_SUPPLICANT", "WIFI_STOP_SUPPLICANT",
        "WIFI_CONNECT_SUPPLICANT", "WIFI_CLOSE_SUPPLICANT", "WIFI_WAIT_EVENT"};
    enum {CTRL_EVENT_LINK_SPEED = 100, CTRL_EVENT_DRIVER_STATE,
        CTRL_EVENT_EAP_FAILURE, CTRL_EVENT_BSS_ADDED, CTRL_EVENT_BSS_REMOVED};
    static struct {
        const char *name;
        int event;
    } event_map[] = {
        // We make a LOT of assumptions about the CONNECTED format
        {"CTRL-EVENT-CONNECTED - Connection to ", NETWORK_CONNECTION_EVENT},
        {"CTRL-EVENT-DISCONNECTED ", NETWORK_DISCONNECTION_EVENT},
        {"CTRL-EVENT-STATE-CHANGE ", SUP_STATE_CHANGE_EVENT},
        {"CTRL-EVENT-SCAN-RESULTS ", SUP_SCAN_RESULTS_EVENT},
        {"CTRL-EVENT-LINK-SPEED ", CTRL_EVENT_LINK_SPEED},
        {"CTRL-EVENT-TERMINATING ", SUP_DISCONNECTION_EVENT},
        {"CTRL-EVENT-DRIVER-STATE ", CTRL_EVENT_DRIVER_STATE},
        {"CTRL-EVENT-EAP-FAILURE ", CTRL_EVENT_EAP_FAILURE},
        {"CTRL-EVENT-BSS-ADDED ", CTRL_EVENT_BSS_ADDED},
        {"CTRL-EVENT-BSS-REMOVED ", CTRL_EVENT_BSS_REMOVED},
        {"WPA:", AUTHENTICATION_FAILURE_EVENT}, {NULL, 0} };
    int ret = 0;
    char request_wifibuf[BUF_SIZE];  // Will store a UTF string

    SLOGD("request_wifi: %s\n", reqname[request]);
    switch (request) {
    case WIFI_LOAD_DRIVER:
        return wifi_load_driver();
    case WIFI_UNLOAD_DRIVER:
        return wifi_unload_driver();
    case WIFI_IS_DRIVER_LOADED:
        return is_wifi_driver_loaded();
    case WIFI_START_SUPPLICANT:
        return wifi_start_supplicant(0);
    case WIFI_STOP_SUPPLICANT:
        return wifi_stop_supplicant();
    case WIFI_CONNECT_SUPPLICANT:
        return wifi_connect_to_supplicant(mInterface.string());
    case WIFI_CLOSE_SUPPLICANT:
        wifi_close_supplicant_connection(mInterface.string());
        break;
    case WIFI_WAIT_EVENT:
        if (wifi_wait_for_event(mInterface.string(), request_wifibuf, sizeof(request_wifibuf)) > 0) {
            char *buf = request_wifibuf;
            int event = 0, len = 0;
            SLOGV("##### %s\n", request_wifibuf);
            while (1) {
                len = strlen(event_map[event].name);
                if (!strncmp(buf, event_map[event].name, len))
                    break;
                event++;
                if (!event_map[event].name) {
                    SLOGV(".....Unprocessed supplicant event: %s\n", buf);
                    return 0;
                }
            }
            buf += len;
            event = event_map[event].event;
            switch (event) {
            case NETWORK_CONNECTION_EVENT: {
                /* Regex pattern for extracting an Ethernet-style MAC address from a string.
                 * Matches a strings like the following:<pre>
                 * CTRL-EVENT-CONNECTED - Connection to 00:1e:58:ec:d5:6d completed (reauth) [id=1 id_str=]</pre> */
                char *p = buf;
                while (*p != ' ' && *p)
                    p++;
                if (!*p) {
                    SLOGV("......unable to find termination of mac address %s\n", buf);
                    return 0;
                }
                String8 macaddr = String8(buf, p - buf);
                buf = p;
                while (*buf != '=' && *buf)
                    buf++;
                if (!*buf) {
                    SLOGV(".....unable to find id= in %s\n", buf);
                    return 0;
                }
                p = ++buf;
                while (isdigit(*p))
                    p++;
                if (*p != ' ') {  //Normally ends with a space
                    SLOGV(".....unable to find termination of id in %s\n", buf);
                    return 0;
                }
                enqueue(new Message(event, atoi(String8(buf, p-buf).string()), 0, macaddr));
                break;
                }
            case SUP_STATE_CHANGE_EVENT: {
                Vector<String8> items = splitString(buf, ' ');
                String8 bssid;
                int network_id = -1;
                int new_state = -1;
                for (size_t i = 0 ; i < items.size() ; i++) {
                    Vector<String8> pair = splitString(items[i],'=');
                    if (pair.size() != 2)
                        continue;
                    if (pair[0] == "BSSID")
                        bssid = pair[1];
                    else if (pair[0] == "id")
                        network_id = atoi(pair[1].string());
                    else if (pair[0] == "state")
                        new_state = atoi(pair[1].string());
                }
                if (new_state == -1)
                    break;
                enqueue(new Message(event, network_id, new_state, bssid));
                break;
                }
            case CTRL_EVENT_LINK_SPEED:
                SLOGV("....link-speed %s\n", buf);
                break;
            case CTRL_EVENT_DRIVER_STATE:
                SLOGV("....driver-state %s\n", buf);
                break;
            case CTRL_EVENT_EAP_FAILURE:
                SLOGV("....eap-failure %s\n", buf);
                break;
            case CTRL_EVENT_BSS_ADDED:
                // SLOGV(".....bss-added %s\n", buf);
                break;
            case CTRL_EVENT_BSS_REMOVED:
                // SLOGV(".....bss-removed %s\n", buf);
                break;
            case NETWORK_DISCONNECTION_EVENT:
            case SUP_SCAN_RESULTS_EVENT:
            case AUTHENTICATION_FAILURE_EVENT:
            case SUP_DISCONNECTION_EVENT:
                enqueue(event);
                break;
            }
        }
    }
    return ret;
}

// Copied from android_net_wifi_Wifi.cpp
String8 WifiStateMachine::doWifiStringCommand(const char *fmt, va_list args)
{
    char buf[BUF_SIZE];
    char reply[4096];
    size_t reply_len = sizeof(reply) - 1;
    int byteCount = vsnprintf(buf, sizeof(buf), fmt, args);
    if (byteCount < 0 || byteCount >= BUF_SIZE
     || ::wifi_command(mInterface.string(), buf, reply, &reply_len))
	reply_len = 0;
    if (reply_len > 0 && reply[reply_len-1] == '\n')
	reply_len--;
    reply[reply_len] = 0;
    // printf("[[[%s]]]\n", reply);
    return String8(reply);
}

String8 WifiStateMachine::doWifiStringCommand(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = doWifiStringCommand(fmt, args);
    va_end(args);
    return result;
}

bool WifiStateMachine::doWifiBooleanCommand(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = doWifiStringCommand(fmt, args);
    va_end(args);
    bool ret = (!strcmp(result, "OK"));
    if (!ret)
        SLOGI("doWifiBooleanCommand:: '%s' failed\n", result.string());
    return ret;
}

/* message class to carry DHCP results */
class DhcpSuccessMessage : public Message {
public:
    DhcpSuccessMessage(const char *in_ipaddr, const char *in_gateway,
               const char *in_dns1, const char *in_dns2, const char *in_server)
    : Message(DHCP_SUCCESS) , ipaddr(in_ipaddr) , gateway(in_gateway)
    , dns1(in_dns1) , dns2(in_dns2) , server(in_server) {}
    String8 ipaddr, gateway, dns1, dns2, server;
};

/* message class to carry extra configuration data for network add/update */
class AddOrUpdateNetworkMessage : public Message {
public:
    AddOrUpdateNetworkMessage(const ConfiguredStation& cs)
    : Message(CMD_ADD_OR_UPDATE_NETWORK) , mConfig(cs) {}
    ConfiguredStation mConfig;
};

// ------------------------------------------------------------
static String8 removeDoubleQuotes(const String8 s)
{
    if (s.size() >= 2 && s[0] == '"' && s[s.size()-1] == '"')
        return String8(s.string() + 1, s.size() - 2);
    return s;
}

void WifiStateMachine::readNetworkVariables(ConfiguredStation& station)
{
    if (station.network_id < 0)
        return;
    String8 p = doWifiStringCommand("GET_NETWORK %d ssid", station.network_id);
    if (p != "FAIL")
        station.ssid = removeDoubleQuotes(p);
    p = doWifiStringCommand("GET_NETWORK %d priority", station.network_id);
    if (p != "FAIL")
        station.priority = atoi(p.string());
    p = doWifiStringCommand("GET_NETWORK %d key_mgmt", station.network_id);
    if (p != "FAIL")
        station.key_mgmt = p;
    else
        station.key_mgmt = "NONE";
    p = doWifiStringCommand("GET_NETWORK %d psk", station.network_id);
    if (p != "FAIL")
        station.pre_shared_key = p;
}

// ------------------------------------------------------------
stateprocess_t WifiStateMachineActions::default_process(Message *message)
{
    switch (message->command()) {
    case CMD_LOAD_DRIVER:
    case CMD_UNLOAD_DRIVER:
    case CMD_START_SUPPLICANT:
    case CMD_STOP_SUPPLICANT:
    case CMD_RSSI_POLL:
    case SUP_CONNECTION_EVENT:
    case SUP_DISCONNECTION_EVENT:
    case CMD_START_SCAN:
    case CMD_ADD_OR_UPDATE_NETWORK:
    case CMD_REMOVE_NETWORK:
    case CMD_SELECT_NETWORK:
    case CMD_ENABLE_NETWORK:
    case CMD_DISABLE_NETWORK:
        break;
    case CMD_ENABLE_RSSI_POLL:
        mEnableRssiPolling = message->arg1() != 0;
        break;
    case CMD_ENABLE_BACKGROUND_SCAN:
        mEnableBackgroundScan = message->arg1() != 0;
        break;
    default:
        SLOGD("...ERROR - unhandled message %s (%d)\n",
         sMessageToString[message->command()], message->command());
        break;
    }
    return SM_HANDLED;
}

/*
  The Driver states refer to the kernel model.  Executing
  "wifi_load_driver()" causes the appropriate kernel model for your
  board to be inserted and executes a firmware loader.  
  This is tied in tightly to the property system, looking at the
  "wlan.driver.status" property to see if the driver has been loaded.
 */
void WifiStateMachineActions::Driver_Loading_enter(void)
{
    mService->BroadcastState(WS_ENABLING);
    if (!request_wifi(WIFI_LOAD_DRIVER))
        enqueue(CMD_LOAD_DRIVER_SUCCESS);
    else {
        mService->BroadcastState(WS_UNKNOWN);
        enqueue(CMD_LOAD_DRIVER_FAILURE);
    }
}

/*
  The WifiStateMachine watches for supplicant messages about wifi
  state and posts them to the state machine.  It runs in its own thread.
  This is strongly based on WifiStateMachine.java.
 */
static int monitorThread(void *arg)
{
    WifiStateMachine *wsm = static_cast<WifiStateMachine *>(arg);
    SLOGV("........#### Starting monitor thread ####\n");
    int i = 0;
    char buf[BUF_SIZE];  // Will store a UTF string
    while (1) {
        if (!wsm->request_wifi(WIFI_CONNECT_SUPPLICANT)) {
            wsm->enqueue(SUP_CONNECTION_EVENT);
            break;
        }
        if (++i > 5) {
            wsm->enqueue(SUP_DISCONNECTION_EVENT);
            return -1;
        }
        usleep(250 * 1000);  // Sleep for 250 ms
    }
    while (wsm->request_wifi(WIFI_WAIT_EVENT) <= 0)
        ;
    return 0;
}

stateprocess_t WifiStateMachineActions::Driver_Loaded_process(Message *message)
{
    if (message->command() == CMD_START_SUPPLICANT) {
        mNetworkInterface->wifiFirmwareReload("STA");
        if (request_wifi(WIFI_START_SUPPLICANT)) {
            transitionTo(DRIVER_UNLOADING_STATE);
            return SM_HANDLED;
        }
        androidCreateThread(monitorThread, this);
    }
    return SM_DEFAULT;
}

void WifiStateMachineActions::Driver_Unloading_enter(void)
{
    if (!request_wifi(WIFI_UNLOAD_DRIVER)) {
        enqueue(CMD_UNLOAD_DRIVER_SUCCESS);
        mService->BroadcastState(WS_DISABLED);
    }
    else {
        enqueue(CMD_UNLOAD_DRIVER_FAILURE);
        mService->BroadcastState(WS_UNKNOWN);
    }
}

/*
  The supplicant states ensure that the wpa_supplicant program is
  running.  This is tied in tightly to the property system, using the
  "init.svc.wpa_supplicant" property to check if it is running.
 */
stateprocess_t WifiStateMachineActions::Supplicant_Starting_process(Message *message)
{
    if (message->command() == SUP_DISCONNECTION_EVENT) {
        if (++mSupplicantRestartCount <= 5) {
            SLOGD("Restarting supplicant\n");
            request_wifi(WIFI_STOP_SUPPLICANT);
            enqueueDelayed(CMD_START_SUPPLICANT, SUPPLICANT_RESTART_INTERVAL_MSECS);
        } else {
            SLOGD("Failed to start supplicant; unloading driver\n");
            mSupplicantRestartCount = 0;
            enqueue(CMD_UNLOAD_DRIVER);
        }
    }
    if (message->command() != SUP_CONNECTION_EVENT)
        return SM_DEFAULT;
    mService->BroadcastState(WS_ENABLED);

    // Returns data = 'Macaddr = XX:XX:XX:XX:XX:XX'
    String8 data = doWifiStringCommand("DRIVER MACADDR");
    if (strncmp("Macaddr = ", data.string(), 10)) {
        SLOGW("Unable to retrieve MAC address in wireless driver\n");
    } else {
        Mutex::Autolock _l(mReadLock);
        mWifiInformation.macaddr = String8(data.string() + 10);
        mService->BroadcastInformation(mWifiInformation);
    }
    {
    Mutex::Autolock _l(mReadLock);
    // ### TODO: This can stall.  Not a good idea
    bool something_changed = false;
    mStationsConfig.clear();
    // printf("......loadConfiguredNetworks()\n");
    String8 listStr = doWifiStringCommand("LIST_NETWORKS");
    Vector<String8> lines = splitString(listStr, '\n');
    // The first line is a header
    for (size_t i = 1 ; i < lines.size() ; i++) {
        Vector<String8> result = splitString(lines[i], '\t');
        ConfiguredStation cs;
        cs.network_id = atoi(result[0].string());
        readNetworkVariables(cs);
        cs.status = ConfiguredStation::ENABLED;
        if (result.size() > 3) {
            if (!strcmp(result[3].string(), "[CURRENT]"))
            cs.status = ConfiguredStation::CURRENT;
            else if (!strcmp(result[3].string(), "[DISABLED]")) {
                cs.status = ConfiguredStation::DISABLED;
                if (doWifiBooleanCommand("ENABLE_NETWORK %d", cs.network_id)) {
                    something_changed = true;
                    cs.status = ConfiguredStation::ENABLED;
                }
            }
        }
        mStationsConfig.push(cs);
    }
    if (something_changed) {
        doWifiBooleanCommand("AP_SCAN 1");
        doWifiBooleanCommand("SAVE_CONFIG");
    }
    }
    mService->BroadcastConfiguredStations(mStationsConfig);
    mSupplicantRestartCount = 0;
    return SM_DEFAULT;
}

int WifiStateMachine::findIndexByNetworkId(int network_id)
{
    for (size_t i = 0 ; i < mStationsConfig.size() ; i++)
        if (mStationsConfig[i].network_id == network_id)
            return i;
    SLOGW("WifiStateMachine::findIndexByNetworkId: network %d doesn't exist\n", network_id);
    return -1;
}

/* This superclass state encapsulates all of:
     DriverStarting, DriverStopping, DriverStarted, DriverStarted
     ScanMode ConnectMode Connecting, Disconnected, Connected, Disconnecting
 */
stateprocess_t WifiStateMachineActions::Supplicant_Started_process(Message *message)
{
    int network_id = message->arg1();
    switch (message->command()) {
    case SUP_DISCONNECTION_EVENT:
        SLOGD("Connection lost, restarting supplicant\n");
        request_wifi(WIFI_STOP_SUPPLICANT);
        request_wifi(WIFI_CLOSE_SUPPLICANT);
        // mNetworkInfo.setIsAvailable(false);
        Supplicant_Started_exit();
        // sendSupplicantConnectionChangedBroadcast(false);
        // mSupplicantStateTracker.sendMessage(CMD_RESET_SUPPLICANT_STATE);
        // mWpsStateMachine.sendMessage(CMD_RESET_WPS_STATE);
        enqueueDelayed(CMD_START_SUPPLICANT, SUPPLICANT_RESTART_INTERVAL_MSECS);
        break;
    case SUP_SCAN_RESULTS_EVENT: {
        Mutex::Autolock _l(mReadLock);
        mScanResultIsPending = false;
        /* bssid / frequency / signal level / flags / ssid
           00:19:e3:33:55:2e2457-64[WPA-PSK-TKIP][WPA2-PSK-TKIP+CCMP][ESS]ADTC
           00:24:d2:92:7e:e42412-65[WEP][ESS]5YPKM
           00:26:62:50:7e:4a2462-89[WEP][ESS]HD253 */
        String8 data = doWifiStringCommand("SCAN_RESULTS");
        mStations.clear();
        Vector<String8> lines = splitString(data.string(), '\n');
        for (size_t i = 1 ; i < lines.size() ; i++) {
            Vector<String8> elements = splitString(lines[i], '\t');
            if (elements.size() < 3 || elements.size() > 5)
                SLOGW("WifiStateMachine::handleScanResults() Illegal data: %s\n", lines[i].string());
            else {
                int frequency = atoi(elements[1].string());
                int rssi      = atoi(elements[2].string());
                String8 flags, ssid;
                if (elements.size() == 5) {
                    flags = elements[3];
                    ssid = elements[4];
                }
                else if (elements.size() == 4) {
                    if (elements[3].string()[0] == '[')
                        flags = elements[3];
                    else
                        ssid = elements[3];
                }
                ssid = trimString(ssid);
                if (!ssid.isEmpty())
                    mStations.push(ScannedStation(elements[0], ssid, flags, frequency, rssi));
            }
        }
        mService->BroadcastScanResults(mStations);
        return SM_HANDLED;
        }
    case CMD_ADD_OR_UPDATE_NETWORK: {
        Mutex::Autolock _l(mReadLock);
        /* It's an update if the station has a valid network_id and
          the SSID values match.
          It's a new station if the network_id = -1 AND the SSID value
          does not already exist in the list.
         */
        const ConfiguredStation& cs = static_cast<AddOrUpdateNetworkMessage *>(message)->mConfig;
        int index = -1;
        network_id = cs.network_id;
        SLOGD("WifiStateMachine::addOrUpdate id=%d ssid=%s\n", network_id, cs.ssid.string());
        if (network_id != -1) {   // It's an update!
            index = findIndexByNetworkId(network_id);
            if (index < 0)
                return SM_HANDLED;
        } else {   // Adding a new station
            for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
                if (mStationsConfig[i].ssid == cs.ssid) {
                    SLOGW("WifiStateMachine::addOrUpdate: Attempting to add ssid=%s"
                    " but that station already exists\n", cs.ssid.string());
                    return SM_HANDLED;
                    }
            }
            String8 s = doWifiStringCommand("ADD_NETWORK");
            if (s.isEmpty() || !isdigit(s.string()[0])) {
                SLOGW("WifiStateMachine::addOrUpdate: Failed to add a network [%s]\n", s.string());
                return SM_HANDLED;
            }
            network_id = atoi(s.string());
            //printf("* Adding new network id=%d\n", network_id);
        }
        // ****************************************
        // TODO:  The preshared key and the SSID need to be properly string-escaped
        // ****************************************
        // Configure the SSID, Key management, Pre-shared key, Priority
        if ((!doWifiBooleanCommand("SET_NETWORK %d ssid \"%s\"", network_id, cs.ssid.string())
         || (!cs.key_mgmt.isEmpty() && !doWifiBooleanCommand("SET_NETWORK %d key_mgmt %s", network_id, cs.key_mgmt.string()))
         || (!cs.pre_shared_key.isEmpty() && cs.pre_shared_key != "*" &&
            !doWifiBooleanCommand("SET_NETWORK %d psk \"%s\"", network_id, cs.pre_shared_key.string()))
         || !doWifiBooleanCommand("SET_NETWORK %d priority %d", network_id, cs.priority))
         && cs.network_id == -1) {  // We were adding a new station
            doWifiBooleanCommand("REMOVE_NETWORK %d", network_id);
            return SM_HANDLED;
        }
        // Save the configuration
        doWifiBooleanCommand("AP_SCAN 1");
        doWifiBooleanCommand("SAVE_CONFIG");
        // We need to re-read the configuration back from the supplicant
        // to correctly update the values that will be displayed to the client.
        if (index == -1) {    // We've added a new station; shove one on the end of the list
            mStationsConfig.push(ConfiguredStation());
            index = mStationsConfig.size() - 1;
        }
        ConfiguredStation& station(mStationsConfig.editItemAt(index));
        station.network_id = network_id;
        if (cs.network_id == -1)
            station.status = ConfiguredStation::DISABLED;
        readNetworkVariables(station);
        }
        /* fall through */
    case CMD_SELECT_NETWORK: {
        /* Fix this network to have the highest priority and disable all others.
           For the moment we'll not worry about too high of a priority */
        Mutex::Autolock _l(mReadLock);
        if (network_id == -1)
            return SM_HANDLED;
        SLOGD("WifiStateMachine::selectNetwork %d\n", network_id);
        int index = findIndexByNetworkId(network_id);
        if (index < 0)
            return SM_HANDLED;
        int p = 0;
        for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
            if (mStationsConfig[i].priority > p)
                p = mStationsConfig[i].priority;
        }
        if (mStationsConfig[index].priority < p) {
            p++;
            if (!doWifiBooleanCommand("SET_NETWORK %d priority %d", network_id, p)) {
                SLOGW("WifiStateMachine::selectNetwork failed to priority %d: %d\n", network_id, p);
                return SM_HANDLED;
            }
            mStationsConfig.editItemAt(index).priority = p;
        }
        }
        /* fall through */
    case CMD_ENABLE_NETWORK: {
        Mutex::Autolock _l(mReadLock);
        bool disable_others = message->command() != CMD_ENABLE_NETWORK || message->arg2() != 0;
        SLOGD("WifiStateMachine::enableNetwork %d (disable_others=%d)\n", network_id, disable_others);
        if (!doWifiBooleanCommand("%s %d",
           (disable_others ? "SELECT_NETWORK" : "ENABLE_NETWORK"), network_id))
            SLOGW("WifiStateMachine::enableNetwork failed for %d\n", network_id);
        else {
            for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
                ConfiguredStation& station(mStationsConfig.editItemAt(i));
                if (station.network_id == network_id)
                    station.status = ConfiguredStation::ENABLED;
                else if (disable_others)
                    station.status = ConfiguredStation::DISABLED;
            }
        }
        goto broadcastresult;
        }
    case CMD_DISABLE_NETWORK: {
        Mutex::Autolock _l(mReadLock);
        SLOGD("WifiStateMachine::disableNetwork %d\n", network_id);
        if (!doWifiBooleanCommand("DISABLE_NETWORK %d", network_id))
            SLOGW("WifiStateMachine::disableNetwork failed for %d\n", network_id);
        else {
            for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
                ConfiguredStation& station(mStationsConfig.editItemAt(i));
                if (station.network_id == network_id)
                    station.status = ConfiguredStation::DISABLED;
            }
        }
        goto broadcastresult;
        }
    case CMD_REMOVE_NETWORK: {
        SLOGV("REMOVING NETWORK %d\n", network_id);
        Mutex::Autolock _l(mReadLock);
        if (!doWifiBooleanCommand("REMOVE_NETWORK %d", network_id)) {
            SLOGI("WifiStateMachine::removeNetwork %d failed\n", network_id);
        } else {
            for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
                const ConfiguredStation& cs = mStationsConfig.itemAt(i);
                if (cs.network_id == network_id) {
                    int status = mStationsConfig.itemAt(i).status;
                    mStationsConfig.removeAt(i);
                    if (status == ConfiguredStation::CURRENT) {
                        /* Enable other networks if we remove the active network */
                        for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
                            ConfiguredStation& cs = mStationsConfig.editItemAt(i);
                            if (cs.status == ConfiguredStation::DISABLED && 
                                doWifiBooleanCommand("ENABLE_NETWORK %d", cs.network_id)) {
                                cs.status = ConfiguredStation::ENABLED;
                            }
                        }
                    }
                    break;
                }
            }
            doWifiBooleanCommand("AP_SCAN 1");
            doWifiBooleanCommand("SAVE_CONFIG");
        }
        goto broadcastresult;
        }
    }
    return SM_DEFAULT;
broadcastresult:
    mService->BroadcastConfiguredStations(mStationsConfig);
    return SM_HANDLED;
}

void WifiStateMachineActions::Supplicant_Started_exit(void)
{
    SLOGV("....Supplicant_Started_exit(): Stop DHCP and clear IP\n");
    ::dhcp_stop(mInterface.string());
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
    mService->BroadcastInformation(mWifiInformation);
    for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
        ConfiguredStation& cs = mStationsConfig.editItemAt(i);
        if (cs.status == ConfiguredStation::CURRENT)
            cs.status = ConfiguredStation::ENABLED;
    }
    mService->BroadcastConfiguredStations(mStationsConfig);
}

void WifiStateMachineActions::Supplicant_Stopping_enter(void)
{
    mService->BroadcastState(WS_DISABLING);
    SLOGD("########### Asking supplicant to terminate\n");
    if (!doWifiBooleanCommand("TERMINATE"))
        request_wifi(WIFI_STOP_SUPPLICANT);
    transitionTo(DRIVER_LOADED_STATE);
}

/*
  The Driver Start/Stop states are all about controlling if the
  interface is running or not (wpa_state=INTERFACE_DISABLED).
  This is changed with the wpa_cli command "driver start" and "driver stop"
 */
void WifiStateMachineActions::Driver_Started_enter(void)
{
    SLOGV(".........driver started state enter....\n");
    // Set country code if available
    // setFrequencyBand();
//    setNetworkDetailedState(DISCONNECTED);
    if (mIsScanMode) {
        doWifiBooleanCommand("AP_SCAN 2");  // SCAN_ONLY_MODE
        doWifiBooleanCommand("DISCONNECT");
        transitionTo(SCAN_MODE_STATE);
    } else {
        doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
        doWifiBooleanCommand("RECONNECT");
        transitionTo(DISCONNECTED_STATE);
    }
}

stateprocess_t WifiStateMachineActions::Driver_Started_process(Message *message)
{
    if (message->command() != CMD_START_SCAN)
        return SM_NOT_HANDLED;
    if (message->arg1())
        doWifiBooleanCommand("DRIVER SCAN-ACTIVE");
    doWifiBooleanCommand("SCAN");
    if (message->arg1())
        doWifiBooleanCommand("DRIVER SCAN-PASSIVE");
    mScanResultIsPending = true;
    return SM_HANDLED;
}

stateprocess_t WifiStateMachineActions::Driver_Stopping_process(Message *message)
{
    if (message->command() == SUP_STATE_CHANGE_EVENT) {
        handleSupplicantStateChange(message);
        if (mWifiInformation.supplicant_state != SupplicantState::INTERFACE_DISABLED)
            return SM_HANDLED;
    }
    return SM_DEFAULT;
}

/*
  The ConnectMode and ScanModes are the 'normal' range of the Wifi
  driver.  In these modes, we know that the kernel modules have been
  loaded, that wpa_supplicant is currently running, and that the
  wpa_supplicant driver has been started.  
  We cycle between being connected, disconnected, or in "scan" mode.
      CONNECT_MODE = 1 SCAN_ONLY_MODE = 2
 */
stateprocess_t WifiStateMachineActions::Connect_Mode_process(Message *message)
{
    switch (message->command()) {
    case AUTHENTICATION_FAILURE_EVENT:
        SLOGV("TODO: Authentication failure\n");
        return SM_HANDLED;
    case SUP_STATE_CHANGE_EVENT:
        SLOGV("...ConnectMode: Supplicant state change event\n");
        handleSupplicantStateChange(message);
        return SM_HANDLED;
    case CMD_DISCONNECT:
        doWifiBooleanCommand("DISCONNECT");
        return SM_HANDLED;
    case CMD_RECONNECT:
        doWifiBooleanCommand("RECONNECT");
        return SM_HANDLED;
    case CMD_REASSOCIATE:
        doWifiBooleanCommand("REASSOCIATE");
        return SM_HANDLED;
    case SUP_SCAN_RESULTS_EVENT:    // Go back to "connect" mode
        doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
        return SM_NOT_HANDLED;
    case CMD_CONNECT_NETWORK: {
        int network_id = message->arg1();
        SLOGV("TODO: WifiStateMachine.Connect_Mode_process(network_id=%d);\n", network_id);
        } return SM_HANDLED;
    case NETWORK_CONNECTION_EVENT: {
        SLOGV("....handleNetworkConnect(%p)\n", message);
        {
        uint32_t prefixLength, lease;
        char ipaddr[PROPERTY_VALUE_MAX], gateway[PROPERTY_VALUE_MAX];
        char dns1[PROPERTY_VALUE_MAX], dns2[PROPERTY_VALUE_MAX];
        char server[PROPERTY_VALUE_MAX], vendorInfo[PROPERTY_VALUE_MAX];

        int result = ::dhcp_do_request(mInterface.string(), ipaddr, gateway,
            &prefixLength, dns1, dns2, server, &lease, vendorInfo);
        if (result)
            enqueue(DHCP_FAILURE);
        else
            enqueue(new DhcpSuccessMessage(ipaddr, gateway, dns1, dns2, server));
        }
        Mutex::Autolock _l(mReadLock);
        mWifiInformation.bssid = message->string();
        mWifiInformation.network_id = message->arg1();
        String8 status = doWifiStringCommand("STATUS");
        SLOGV("....checking status '%s'\n", status.string());
        Vector<String8> lines = splitString(status, '\n');
        for (size_t i=0 ; i < lines.size() ; i++) {
            Vector<String8> pair = splitString(lines[i], '=');
            if (pair.size() == 2 && pair[0] == "ssid")
                mWifiInformation.ssid = pair[1];
        }
        mService->BroadcastInformation(mWifiInformation);
        for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
            ConfiguredStation& cs = mStationsConfig.editItemAt(i);
            if (cs.network_id == mWifiInformation.network_id) {
                if (cs.status == ConfiguredStation::ENABLED)
                    cs.status = ConfiguredStation::CURRENT;
                else
                    SLOGI("WifiStateMachine::networkConnect to disabled station\n");
                break;
            }
        }
        mService->BroadcastConfiguredStations(mStationsConfig);
        }
        break;
    case NETWORK_DISCONNECTION_EVENT:
        Supplicant_Started_exit();
        break;
    }
    return SM_DEFAULT;
}

static bool fixDnsEntry(const char *key, const char *value)
{
    char old[PROPERTY_VALUE_MAX];
    property_get(key, old, "");
    if (strcmp(old, value)) {
        property_set(key, value);
        return true;
    }
    return false;
}

stateprocess_t WifiStateMachineActions::Connecting_process(Message *message)
{
    if (message->command() == DHCP_SUCCESS) {
        const DhcpSuccessMessage *dmessage = static_cast<DhcpSuccessMessage *>(message);
        SLOGV("Got DHCP ipaddr=%s gateway=%s dns1=%s dns2=%s server=%s\n",
            dmessage->ipaddr.string(), dmessage->gateway.string(), dmessage->dns1.string(),
            dmessage->dns2.string(), dmessage->server.string());
        // Set a default route
        mNetworkInterface->setDefaultRoute(dmessage->gateway.string());
        // Update property system with DNS data for the resolver
        if (fixDnsEntry("net.dns1", dmessage->dns1.string())
         || fixDnsEntry("net.dns2", dmessage->dns2.string())) {
            // ### TODO: Check to make sure they aren't local addresses
            mNetworkInterface->setDns(dmessage->dns1.string(), dmessage->dns2.string());
        }
        Mutex::Autolock _l(mReadLock);
        mWifiInformation.ipaddr = dmessage->ipaddr;
        mService->BroadcastInformation(mWifiInformation);
    }
    return SM_DEFAULT;
}

void WifiStateMachineActions::Connected_enter(void)
{
    if (mEnableRssiPolling)
        enqueueDelayed(CMD_RSSI_POLL, RSSI_POLL_INTERVAL_MSECS);
}

stateprocess_t WifiStateMachineActions::Connected_process(Message *message)
{
    switch (message->command()) {
    case CMD_DISCONNECT:
        doWifiBooleanCommand("DISCONNECT");
        break;
    case CMD_START_SCAN:
        doWifiBooleanCommand("AP_SCAN 2");   // SCAN_ONLY_MODE
        return SM_NOT_HANDLED;
    case CMD_ENABLE_RSSI_POLL:
        mEnableRssiPolling = (message->arg1());
        /* fall through */
    case CMD_RSSI_POLL:
        if (mEnableRssiPolling) {
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
            mService->BroadcastRssi(mWifiInformation.rssi);
            if (link_speed != -1) {
                mWifiInformation.link_speed = link_speed;
                mService->BroadcastLinkSpeed(link_speed);
            }
            mService->BroadcastInformation(mWifiInformation);
            enqueueDelayed(CMD_RSSI_POLL, RSSI_POLL_INTERVAL_MSECS);
        }
        return SM_HANDLED;
    }
    return SM_DEFAULT;
}

void WifiStateMachineActions::Connected_exit(void)
{
    if (mScanResultIsPending)
        doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
}

stateprocess_t WifiStateMachineActions::Disconnecting_process(Message *message)
{
    if (message->command() == SUP_STATE_CHANGE_EVENT)
        Supplicant_Started_exit();
    return SM_DEFAULT;
}

void WifiStateMachineActions::Disconnected_enter(void)
{
    if (mEnableBackgroundScan && !mScanResultIsPending)
        doWifiBooleanCommand("DRIVER BGSCAN-START");
}

stateprocess_t WifiStateMachineActions::Disconnected_process(Message *message)
{
    switch (message->command()) {
    case CMD_START_SCAN:
        /* Disable background scan temporarily during a regular scan */
        if (mEnableBackgroundScan)
            doWifiBooleanCommand("DRIVER BGSCAN-STOP");
        return SM_NOT_HANDLED;  // Handle in parent state

    case CMD_ENABLE_BACKGROUND_SCAN:
        mEnableBackgroundScan = message->arg1() != 0;
        doWifiBooleanCommand(mEnableBackgroundScan ? "DRIVER BGSCAN-START" : "DRIVER BGSCAN-STOP");
        return SM_HANDLED;

    case SUP_SCAN_RESULTS_EVENT:
        /* Re-enable background scan when a pending scan result is received */
        if (mEnableBackgroundScan && mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        return SM_NOT_HANDLED;  // Handle in parent state
    }
    return SM_NOT_HANDLED;
}

void WifiStateMachineActions::Disconnected_exit(void)
{
    if (mEnableBackgroundScan)
        doWifiBooleanCommand("DRIVER BGSCAN-STOP");
}

stateprocess_t WifiStateMachineActions::Driver_Failed_process(Message *m)
{
    return SM_HANDLED;
}

stateprocess_t WifiStateMachineActions::sm_default_process(Message *m)
{
    return SM_DEFAULT;
}

// ------------------------------------------------------------
WifiStateMachine::WifiStateMachine(const char *interface, WifiService *servicep)
    : mInterface(interface)
    , mIsScanMode(false)
    , mEnableRssiPolling(true)
    , mEnableBackgroundScan(false)
    , mScanResultIsPending(false)
    , mService(servicep)
{
    mNetworkInterface = new NetworkInterface(this, mInterface);
    ::dhcp_stop(mInterface.string());

    // Ensure we've terminated DHCP and the wpa_supplicant
    // The DHCP state machine ensures that the old dhcp is stopped
    SLOGV("Stopping supplicant, result=%d\n", request_wifi(WIFI_STOP_SUPPLICANT));

    ADD_ITEMS(mStateMap);
    //transitionTo(INITIAL_STATE);
    if (request_wifi(WIFI_IS_DRIVER_LOADED))
        transitionTo(DRIVER_LOADED_STATE);
    else
        transitionTo(DRIVER_UNLOADED_STATE);
    /* Connect to a CommandListener daemon (e.g. netd)
      This is a simplified bit of code which expects to only be
      called by a SINGLE thread (no multiplexing of requests from multiple threads).  */
    status_t result = mNetworkInterface->run("NetworkInterface", PRIORITY_NORMAL);
    // INVALID_OPERATION is returned if the thread is already running
    if (result != NO_ERROR && result != INVALID_OPERATION) {  
	SLOGW("Could not start WifiMonitor thread due to error %d\n", result);
	exit(1);
    }
    SLOGV("...................WifiStateMachine::startRunning()\n");
    result = run("WifiStateMachine", PRIORITY_NORMAL);
    LOG_ALWAYS_FATAL_IF(result, "Could not start WifiStateMachine thread due to error %d\n", result);
}

void WifiStateMachine::invoke_enter(ENTER_EXIT_PROTO fn)
{
    typedef void (WifiStateMachineActions::*WENTER_EXIT_PROTO)(void);
    printf ("HIGHE\n");
    if (fn)
        (static_cast<WifiStateMachineActions *>(this)->*static_cast<WENTER_EXIT_PROTO>(fn))();
}
stateprocess_t WifiStateMachine::invoke_process(PROCESS_PROTO fn, Message *m)
{
    typedef stateprocess_t (WifiStateMachineActions::*WPROCESS_PROTO)(Message *);
    printf ("HIGHP\n");
    if (!fn)
        return SM_NOT_HANDLED;
    return (static_cast<WifiStateMachineActions *>(this)->*static_cast<WPROCESS_PROTO>(fn))(m);
}

void WifiStateMachine::Register(const sp<IWifiClient>& client, int flags)
{
    Mutex::Autolock _l(mReadLock);

    // We don't preemptively send scandata - it's probably old anyways
    if (flags & WIFI_CLIENT_FLAG_CONFIGURED_STATIONS)
	client->ConfiguredStations(mStationsConfig);
    if (flags & WIFI_CLIENT_FLAG_INFORMATION)
	client->Information(mWifiInformation);
    // We don't preemptively send rssi or link speed data
}

void WifiStateMachine::handleSupplicantStateChange(Message *message)
{
//    SLOGV("....TODO: handleSupplicantStatechange\n");
    Mutex::Autolock _l(mReadLock);
    mWifiInformation.supplicant_state = message->arg2();
    mWifiInformation.network_id = SupplicantState::INVALID;
    if (SupplicantState::isConnecting(mWifiInformation.supplicant_state))
        mWifiInformation.network_id = message->arg1();
    if (mWifiInformation.supplicant_state == SupplicantState::ASSOCIATING)
        mWifiInformation.bssid = message->string();
    mService->BroadcastInformation(mWifiInformation);
}

const char * WifiStateMachine::msgStr(int msg_id)
{
    return sMessageToString[msg_id];
}

void WifiStateMachine::enqueue_network_update(const ConfiguredStation& cs)
{
    enqueue(new AddOrUpdateNetworkMessage(cs));
}

int xsockets[2];
char xbuf[1024];

void xopen()
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xsockets) < 0) {
        perror("opening stream socket pair");
        exit(1);
    }
}

void xwrite()
{
        if (write(xsockets[1], xbuf, sizeof(xbuf)) < 0)
            perror("writing stream message");
        close(xsockets[1]);
}

void xread()
{
        if (read(xsockets[0], xbuf, 1024) < 0)
            perror("reading stream message");
        printf("-->%s\n", xbuf);
        close(xsockets[0]);
}


};  // namespace android
