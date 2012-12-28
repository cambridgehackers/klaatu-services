/*
  Wifi state machine and interface.
  This object should be placed in its own thread

  This is strongly based on the WifiStateMachine.java code
  in Android.
 */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <hardware_legacy/wifi.h>
#include <netutils/dhcp.h>
#include <utils/String8.h>
#define BIT(x) (1 << (x))      /* needed for wpa supplicant defs.h */
#include <src/common/defs.h>   /* WPA_xxx names from external/wpa_supplicant_x */

#include "WifiDebug.h"
#define FSM_ACTION_CODE
#include "WifiStateMachine.h"
#include "StringUtils.h"
#include "WifiService.h"

namespace android {

static const int BUF_SIZE=256;
static const int RSSI_POLL_INTERVAL_MSECS = 3000;
static const int SUPPLICANT_RESTART_INTERVAL_MSECS = 5000;

/* message class to carry DHCP results */
class DhcpResultMessage : public Message {
public:
    DhcpResultMessage(const char *in_ipaddr, const char *in_gateway,
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

static int extractSequence(const char *buf)
{
    int result = 0;
    const char *p = buf;
    while (*p && isdigit(*p))
        result = result * 10 + (*p++ - '0');
    return (p > buf) ? result : -1 ;
}

// All messages from the server should have a 3 digit numeric code followed
// by a sequence number and a text string
static int extractCode(const char *buf)
{
    if (isdigit(buf[0]) && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == ' ')
        return extractSequence(buf);
    return -1;
}

static int seqno;
String8 WifiStateMachine::ncommand(const char *fmt, ...)
{
    String8 response;
    int code = -1;
    char buf[BUF_SIZE];
    char *p = buf;
    va_list args;

    Mutex::Autolock _l(mLock);
    mResponseQueue.clear();
    seqno = ++mSequenceNumber;
    va_start(args, fmt);
    snprintf(p, sizeof(buf), "%d ", seqno);
    p += strlen(p);
    vsnprintf(p, sizeof(buf) - (p - buf), fmt, args);
    va_end(args);

    //String8 message = String8::format("%d %s", seqno, buf);
    SLOGV(".....Netd:          '%s'\n", buf);
    int len = ::write(mFd, buf, strlen(buf) + 1);
    if (len < 0) {
        perror("Unable to write to daemon socket");
        exit(1);
    }
    while (code < 200 || code >= 600) {
        while (mResponseQueue.size() == 0)
            mCondition.wait(mLock);
        response = mResponseQueue[0];
        mResponseQueue.removeAt(0);
        code = extractCode(response.string());
        SLOGV(".....Netd: resp '%s'", response.string());
    }
    return response;
}

bool WifiStateMachine::process_indication()
{
    int count = ::read(mFd, indication_buf + indication_start, sizeof(indication_buf) - indication_start);
    if (count < 0) {
        perror("Error reading daemon socket");
        return true;
    }
    count += indication_start;
    indication_start = 0;
    for (int i = 0 ; i < count ; i++) {
        if (!indication_buf[i]) {
            Mutex::Autolock _l(mLock);
            mResponseQueue.push(String8(indication_buf + indication_start, i - indication_start));
            mCondition.signal();
            indication_start = i + 1;
        }
    }
    if (indication_start < count) 
        memcpy(indication_buf, indication_buf+indication_start, count - indication_start);
    indication_start = count - indication_start;
    return false;
}

int WifiStateMachine::request_wifi(int request)
{
    static const char *reqname[] = {"",
        "WIFI_LOAD_DRIVER", "WIFI_UNLOAD_DRIVER", "WIFI_IS_DRIVER_LOADED",
        "WIFI_START_SUPPLICANT", "WIFI_STOP_SUPPLICANT",
        "WIFI_CONNECT_SUPPLICANT", "WIFI_CLOSE_SUPPLICANT", "WIFI_WAIT_EVENT",
        "DHCP_STOP", "DHCP_DO_REQUEST"};
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
    char rbuf[BUF_SIZE];  // Will store a UTF string

    if (request != WIFI_WAIT_EVENT)
        SLOGD("....REQ: %s\n", reqname[request]);
    switch (request) {
    case DHCP_STOP:
        return ::dhcp_stop(mInterface.string());
    case DHCP_DO_REQUEST: {
        uint32_t prefixLength, lease;
        char ipaddr[PROPERTY_VALUE_MAX], gateway[PROPERTY_VALUE_MAX];
        char dns1[PROPERTY_VALUE_MAX], dns2[PROPERTY_VALUE_MAX];
        char server[PROPERTY_VALUE_MAX], vendorInfo[PROPERTY_VALUE_MAX];

        int result = ::dhcp_do_request(mInterface.string(), ipaddr, gateway,
            &prefixLength, dns1, dns2, server, &lease, vendorInfo);
        if (result)
            enqueue(DHCP_FAILURE);
        else
            enqueue(new DhcpResultMessage(ipaddr, gateway, dns1, dns2, server));
        break;
        }
    case WIFI_LOAD_DRIVER:
        /*
          The Driver states refer to the kernel model.  Executing
          "wifi_load_driver()" causes the appropriate kernel model for your
          board to be inserted and executes a firmware loader.  
          This is tied in tightly to the property system, looking at the
          "wlan.driver.status" property to see if the driver has been loaded.
         */
        ret = wifi_load_driver();
        enqueue(!ret ? CMD_LOAD_DRIVER_SUCCESS : CMD_LOAD_DRIVER_FAILURE);
        break;
    case WIFI_UNLOAD_DRIVER:
        ret = wifi_unload_driver();
        enqueue(!ret ? CMD_UNLOAD_DRIVER_SUCCESS : CMD_UNLOAD_DRIVER_FAILURE);
        break;
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
        if (wifi_wait_for_event(mInterface.string(), rbuf, sizeof(rbuf)) > 0) {
            char *buf = rbuf;
            int event = 0, len = 0;
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
            if (event != CTRL_EVENT_BSS_ADDED && event != CTRL_EVENT_BSS_REMOVED)
                SLOGV(".....EVENT: '%s' event %s\n", rbuf, msgStr(event));
            switch (event) {
            case NETWORK_CONNECTION_EVENT: {
                /* Regex pattern for extracting an Ethernet-style MAC address from a string.
                 * Matches a strings like the following:
                 * 'CTRL-EVENT-CONNECTED - Connection to 00:1e:58:ec:d5:6d completed (reauth) [id=1 id_str=]' */
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
                    SLOGV(".....unable to find id= in %s\n", p);
                    return 0;
                }
                p = ++buf;
                while (isdigit(*p))
                    p++;
                if (*p != ' ')
                    SLOGV(".....unable to find termination of id in %s\n", buf);
                else
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
            case CTRL_EVENT_DRIVER_STATE:
            case CTRL_EVENT_EAP_FAILURE:
            case CTRL_EVENT_BSS_ADDED:
            case CTRL_EVENT_BSS_REMOVED:
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
    SLOGV(".....Command: %s\n", buf);
    if (byteCount < 0 || byteCount >= BUF_SIZE
     || ::wifi_command(mInterface.string(), buf, reply, &reply_len))
        reply_len = 0;
    if (reply_len > 0 && reply[reply_len-1] == '\n')
        reply_len--;
    reply[reply_len] = 0;
    //SLOGV(".....Reply: %s\n", reply);
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
        SLOGI(".....doWifiBooleanCommand:: '%s' failed\n", result.string());
    return ret;
}

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
    while (wsm->request_wifi(WifiStateMachine::WIFI_CONNECT_SUPPLICANT)) {
        if (++i > 5) {
            wsm->enqueue(SUP_DISCONNECTION_EVENT);
            return -1;
        }
        usleep(250 * 1000);  // Sleep for 250 ms
    }
    wsm->enqueue(SUP_CONNECTION_EVENT);
    while (wsm->request_wifi(WifiStateMachine::WIFI_WAIT_EVENT) <= 0)
        ;
    return 0;
}

void WifiStateMachine::setInterfaceState(int astate) 
{
    static const char *sname[] = {"down", "up"};
    struct {
        String8 hwaddr;
        String8 ipaddr;
        int prefixLength;
        String8 flags;
    } ifcfg;
    int code = 0;

    String8 data = ncommand("interface getcfg %s", mInterface.string());
    Vector<String8> response;
    code = extractCode(data.string());
    if (code < 0) 
        SLOGE("Failed to extract valid code from '%s'", data.string());
    else {
        int s = extractSequence(data.string() + 4);
        if (s < 0)
            SLOGE("Failed to extract valid sequence from '%s'", data.string());
        else if (s != seqno)
            SLOGE("Sequence mismatch %d (should be %d)", s, seqno);
        else {
            SLOGV(".....WifiStateMachine::response: %d", code);
            if (code > 0)
                response.push(data);
        }
    }
    if (code != 213 || response.size() == 0) {
        SLOGW("...can't get interface config code=%d\n", code);
        return;
    }
    // Rsp xx:xx:xx:xx:xx:xx yyy.yyy.yyy.yyy zzz [flag1 flag2 flag3]
    SLOGV(".......parsing: %s\n", response[0].string());
    Vector<String8> elements = splitString(response[0], ' ', 5);
    if (elements.size() != 5) {
        SLOGW("....bad split of interface cmd '%s'\n", response[0].string());
        return;
    }
    ifcfg.hwaddr = elements[1];
    ifcfg.ipaddr = elements[2];
    ifcfg.prefixLength = atoi(elements[3].string());
    ifcfg.flags = elements[4];
    SLOGV(".......ifcfg hwaddr='%s' ipaddr='%s' prefixlen=%d flags='%s'\n",
         ifcfg.hwaddr.string(), ifcfg.ipaddr.string(), ifcfg.prefixLength, ifcfg.flags.string());
    ifcfg.flags = replaceString(ifcfg.flags, sname[1 - astate], sname[astate]);
    data = ncommand("interface setcfg %s %s %d %s", mInterface.string(),
        ifcfg.ipaddr.string(), ifcfg.prefixLength, ifcfg.flags.string());
    code = extractCode(data.string());
    if (!(code >= 200))
        SLOGW("WifiStateMachine::setInterfaceState(): Unable to set interface %s\n", 
             mInterface.string());
}

static void restartSupplicant(WifiStateMachine *wsm)
{
    SLOGD("Restarting supplicant\n");
    wsm->request_wifi(WifiStateMachine::WIFI_STOP_SUPPLICANT);
    wsm->enqueueDelayed(CMD_START_SUPPLICANT, SUPPLICANT_RESTART_INTERVAL_MSECS);
}

int WifiStateMachine::findIndexByNetworkId(int network_id)
{
    for (size_t i = 0 ; i < mStationsConfig.size() ; i++)
        if (mStationsConfig[i].network_id == network_id)
            return i;
    SLOGW("WifiStateMachine::findIndexByNetworkId: network %d doesn't exist\n", network_id);
    return -1;
}

void WifiStateMachine::setStatus(const char *command, int network_id, ConfiguredStation::Status astatus)
{
    Mutex::Autolock _l(mReadLock);
    if (doWifiBooleanCommand(command, network_id)) {
        for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
            ConfiguredStation& station(mStationsConfig.editItemAt(i));
            if (station.network_id == network_id) {
                if (strncmp(command, "REMOVE_NETWORK", strlen("REMOVE_NETWORK")))
                    station.status = astatus;
                else {
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
            else if (!strncmp(command, "SELECT_NETWORK", strlen("SELECT_NETWORK")))
                station.status = ConfiguredStation::DISABLED;
        }
        if (!strncmp(command, "REMOVE_NETWORK", strlen("REMOVE_NETWORK"))) {
            doWifiBooleanCommand("AP_SCAN 1");
            doWifiBooleanCommand("SAVE_CONFIG");
        }
    }
    mService->BroadcastConfiguredStations(mStationsConfig);
}

void WifiStateMachine::disable_interface(void)
{
    request_wifi(DHCP_STOP);
    ncommand("interface clearaddrs %s", mInterface.string());
    // Update the Wifi Information visible to the user
    Mutex::Autolock _l(mReadLock);
    mWifiInformation.ipaddr = "";
    mWifiInformation.bssid = "";
    mWifiInformation.ssid = "";
    mWifiInformation.network_id = -1;
    mWifiInformation.supplicant_state = WPA_INTERFACE_DISABLED;
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

void WifiStateMachine::start_scan(bool aactive)
{
    if (aactive)
        doWifiBooleanCommand("DRIVER SCAN-ACTIVE");
    doWifiBooleanCommand("SCAN");
    if (aactive)
        doWifiBooleanCommand("DRIVER SCAN-PASSIVE");
    mScanResultIsPending = true;
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

static int network_cb(void *arg)
{
    WifiStateMachine *wsm = static_cast<WifiStateMachine *>(arg);

    while (!wsm->process_indication())
        ;
    return 0;
}
// ------------------------------------------------------------
WifiStateMachine::WifiStateMachine(const char *interface, WifiService *servicep)
    : mInterface(interface)
    , mEnableRssiPolling(true)
    , mEnableBackgroundScan(false)
    , mScanResultIsPending(false)
    , mService(servicep)
{
    mSequenceNumber = 0;
    indication_start = 0;
    mFd = socket_local_client("netd", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
    if (mFd < 0) {
        SLOGW("Could not start connection to socket %s due to error %d\n", "netd", mFd);
        exit(1);
    }
    request_wifi(DHCP_STOP);
    request_wifi(WIFI_STOP_SUPPLICANT);
    ADD_ITEMS(mStateMap);
    if (request_wifi(WIFI_IS_DRIVER_LOADED))
        transitionTo(DRIVER_LOADED_STATE);
    else
        transitionTo(DRIVER_UNLOADED_STATE);
    /* Connect to a CommandListener daemon (e.g. netd)
      This is a simplified bit of code which expects to only be
      called by a SINGLE thread (no multiplexing of requests from multiple threads).  */
    androidCreateThread(network_cb, this);
    SLOGV("...................WifiStateMachine::startRunning()\n");
    status_t result = run("WifiStateMachine", PRIORITY_NORMAL);
    LOG_ALWAYS_FATAL_IF(result, "Could not start WifiStateMachine thread due to error %d\n", result);
    SLOGV("...................WifiStateMachine::statemachine running()\n");
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

static bool isConnecting(int state)
{
    switch (state) {
    case WPA_ASSOCIATING: case WPA_AUTHENTICATING: case WPA_ASSOCIATED:
    case WPA_4WAY_HANDSHAKE: case WPA_GROUP_HANDSHAKE: case WPA_COMPLETED:
        return true;
    }
    return false;
}

void WifiStateMachine::handleSupplicantStateChange(Message *message)
{
    Mutex::Autolock _l(mReadLock);
    mWifiInformation.supplicant_state = message->arg2();
    mWifiInformation.network_id = -1;
    if (isConnecting(mWifiInformation.supplicant_state))
        mWifiInformation.network_id = message->arg1();
    if (mWifiInformation.supplicant_state == WPA_ASSOCIATING)
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

void WifiStateMachine::flushDnsCache() 
{
    ncommand("resolver flushif %s", mInterface.string());
    ncommand("resolver flushdefaultif");
}

// ------------------------------------------------------------
/*
  The supplicant states ensure that the wpa_supplicant program is
  running.  This is tied in tightly to the property system, using the
  "init.svc.wpa_supplicant" property to check if it is running.
 */
stateprocess_t WifiStateMachineActions::Supplicant_Starting_process(Message *message)
{
    switch (message->command()) {
    case SUP_DISCONNECTION_EVENT:
        if (++mSupplicantRestartCount <= 5)
            restartSupplicant(this);
        else {
            SLOGD("Failed to start supplicant; unloading driver\n");
            mSupplicantRestartCount = 0;
            enqueue(CMD_UNLOAD_DRIVER);
        }
        break;
    case SUP_STATE_CHANGE_EVENT:
        if (mEnableBackgroundScan && !mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        return SM_HANDLED;
    }
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachineActions::Supplicant_Started_process(Message *message)
{
    switch (message->command()) {
    case SUP_DISCONNECTION_EVENT:
        restartSupplicant(this);
        request_wifi(WIFI_CLOSE_SUPPLICANT);
        // mNetworkInfo.setIsAvailable(false);
        disable_interface();
        // sendSupplicantConnectionChangedBroadcast(false);
        // mSupplicantStateTracker.sendMessage(CMD_RESET_SUPPLICANT_STATE);
        // mWpsStateMachine.sendMessage(CMD_RESET_WPS_STATE);
        break;
    case SUP_SCAN_RESULTS_EVENT:
        return SM_HANDLED;
    case CMD_STOP_SUPPLICANT:
        disable_interface();
        break;
    }
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachineActions::Driver_Started_process(Message *message)
{
    switch (message->command()) {
    case CMD_START_SCAN:
        start_scan(message->arg1() != 0);
        if (mEnableBackgroundScan && !mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        return SM_HANDLED;
    case SUP_SCAN_RESULTS_EVENT:
    case SUP_STATE_CHANGE_EVENT:
        return SM_HANDLED;
    }
    return SM_NOT_HANDLED;
}
stateprocess_t WifiStateMachineActions::Scan_Mode_process(Message *message)
{
    return Driver_Started_process(message);
}

stateprocess_t WifiStateMachineActions::Connect_Mode_process(Message *message)
{
    switch (message->command()) {
    case NETWORK_DISCONNECTION_EVENT:
        if (mEnableBackgroundScan && !mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        break;
    case SUP_STATE_CHANGE_EVENT:
        if (mEnableBackgroundScan && !mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        return SM_HANDLED;
    case CMD_DISCONNECT:
        return SM_HANDLED;
    case SUP_SCAN_RESULTS_EVENT:    // Go back to "connect" mode
        doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
        return SM_HANDLED;
    case CMD_START_SCAN:
        start_scan(message->arg1() != 0);
        return SM_HANDLED;
    case SUP_DISCONNECTION_EVENT:
        if (++mSupplicantRestartCount <= 5)
            restartSupplicant(this);
        else {
            SLOGD("Failed to start supplicant; unloading driver\n");
            mSupplicantRestartCount = 0;
            enqueue(CMD_UNLOAD_DRIVER);
        }
        break;
    }
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachineActions::Connected_process(Message *message)
{
    switch (message->command()) {
    case CMD_START_SCAN:
        doWifiBooleanCommand("AP_SCAN 2");   // SCAN_ONLY_MODE
        start_scan(message->arg1() != 0);
        return SM_HANDLED;
    case SUP_STATE_CHANGE_EVENT:
        if (mEnableBackgroundScan && !mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        return SM_HANDLED;
    case SUP_SCAN_RESULTS_EVENT:    // Go back to "connect" mode
        doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
        return SM_HANDLED;
    case DHCP_FAILURE:
    case CMD_DISCONNECT:
        if (mScanResultIsPending)
            doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
        break;
    }
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachineActions::Disconnected_process(Message *message)
{
    switch (message->command()) {
    case CMD_START_SCAN:
        /* Disable background scan temporarily during a regular scan */
        if (mEnableBackgroundScan)
            doWifiBooleanCommand("DRIVER BGSCAN-STOP");
        start_scan(message->arg1() != 0);
        return SM_HANDLED;
    case CMD_ENABLE_BACKGROUND_SCAN:
        doWifiBooleanCommand(mEnableBackgroundScan ? "DRIVER BGSCAN-START" : "DRIVER BGSCAN-STOP");
        return SM_HANDLED;
    case SUP_SCAN_RESULTS_EVENT:
        /* Re-enable background scan when a pending scan result is received */
        if (mEnableBackgroundScan && mScanResultIsPending)
            doWifiBooleanCommand("DRIVER BGSCAN-START");
        break;
    }
    return SM_NOT_HANDLED;
}

stateprocess_t WifiStateMachineActions::Supplicant_Stopping_process(Message *message)
{
    mService->BroadcastState(WS_DISABLING);
    if (!doWifiBooleanCommand("TERMINATE"))
        request_wifi(WIFI_STOP_SUPPLICANT);
    transitionTo(DRIVER_LOADED_STATE);
    return SM_HANDLED;
}

stateprocess_t WifiStateMachineActions::Driver_Stopping_process(Message *message)
{
    if (message->command() == SUP_STATE_CHANGE_EVENT
     && mWifiInformation.supplicant_state != WPA_INTERFACE_DISABLED)
        return SM_HANDLED;
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachineActions::Disconnecting_process(Message *message)
{
    if (message->command() == SUP_STATE_CHANGE_EVENT)
        disable_interface();
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachineActions::Driver_Failed_process(Message *m)
{
    return SM_HANDLED;
}

stateprocess_t WifiStateMachineActions::sm_default_process(Message *m)
{
    return SM_DEFAULT;
}

stateprocess_t WifiStateMachine::invoke_process(int state, Message *message, STATE_TABLE_TYPE *state_table)
{
    typedef stateprocess_t (WifiStateMachineActions::*WPROCESS_PROTO)(Message *);
    stateprocess_t result = SM_NOT_HANDLED;
    int network_id = message->arg1();

    SLOGV("......Start processing message %s (%d) in state %s\n", msgStr(message->command()),
        message->command(), state_table[state].name);
    switch (message->command()) {
    case AUTHENTICATION_FAILURE_EVENT:
        SLOGV("TODO: Authentication failure\n");
        result = SM_HANDLED;
        break;
    case CMD_LOAD_DRIVER:
        mService->BroadcastState(WS_ENABLING);
        if (request_wifi(WIFI_LOAD_DRIVER))
            mService->BroadcastState(WS_UNKNOWN);
        break;
    case CMD_UNLOAD_DRIVER:
        mService->BroadcastState(request_wifi(WIFI_UNLOAD_DRIVER) ? WS_UNKNOWN : WS_DISABLED);
        break;
    case CMD_ENABLE_RSSI_POLL:
        mEnableRssiPolling = message->arg1() != 0;
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
        result = SM_HANDLED;
        break;
    case CMD_ENABLE_BACKGROUND_SCAN:
        mEnableBackgroundScan = message->arg1() != 0;
        break;
    case NETWORK_DISCONNECTION_EVENT:
        disable_interface();
        break;
    case SUP_STATE_CHANGE_EVENT:
        handleSupplicantStateChange(message);
        break;
    case CMD_DISCONNECT:
        doWifiBooleanCommand("DISCONNECT");
        break;
    case CMD_RECONNECT:
        doWifiBooleanCommand("RECONNECT");
        result = SM_HANDLED;
        break;
    case CMD_REASSOCIATE:
        doWifiBooleanCommand("REASSOCIATE");
        result = SM_HANDLED;
        break;
    case NETWORK_CONNECTION_EVENT: {
        request_wifi(DHCP_DO_REQUEST);
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
    case DHCP_SUCCESS: {
        const DhcpResultMessage *dmessage = static_cast<DhcpResultMessage *>(message);
        SLOGV("Got DHCP ipaddr=%s gateway=%s dns1=%s dns2=%s server=%s\n",
            dmessage->ipaddr.string(), dmessage->gateway.string(), dmessage->dns1.string(),
            dmessage->dns2.string(), dmessage->server.string());
        // Set a default route
        ncommand("interface route add %s default 0.0.0.0 0 %s", mInterface.string(), dmessage->gateway.string());
        // Update property system with DNS data for the resolver
        if (fixDnsEntry("net.dns1", dmessage->dns1.string())
         || fixDnsEntry("net.dns2", dmessage->dns2.string())) {
            // ### TODO: Check to make sure they aren't local addresses
            const char *dns1 = dmessage->dns1.string();
            const char *dns2 = dmessage->dns2.string();
            if (!strcmp(dns1, "127.0.0.1"))
                dns1 = "";
            if (!strcmp(dns2, "127.0.0.1"))
                dns2 = "";
            ncommand("resolver setifdns %s %s %s", mInterface.string(), dns1, dns2);
            ncommand("resolver setifdns %s", mInterface.string());
        }
        {
        Mutex::Autolock _l(mReadLock);
        mWifiInformation.ipaddr = dmessage->ipaddr;
        mService->BroadcastInformation(mWifiInformation);
        }
        if (mEnableRssiPolling)
            enqueueDelayed(CMD_RSSI_POLL, RSSI_POLL_INTERVAL_MSECS);
        break;
        }
    case SUP_CONNECTION_EVENT: {
        bool something_changed = false;
        mService->BroadcastState(WS_ENABLED);
        // Returns data = 'Macaddr = XX:XX:XX:XX:XX:XX'
        String8 data = doWifiStringCommand("DRIVER MACADDR");
        if (strncmp("Macaddr = ", data.string(), 10))
            SLOGW("Unable to retrieve MAC address in wireless driver\n");
        else {
            Mutex::Autolock _l(mReadLock);
            mWifiInformation.macaddr = String8(data.string() + 10);
            mService->BroadcastInformation(mWifiInformation);
        }
        {
        Mutex::Autolock _l(mReadLock);
        // ### TODO: This can stall.  Not a good idea
        mStationsConfig.clear();
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
        }
        if (something_changed) {
            doWifiBooleanCommand("AP_SCAN 1");
            doWifiBooleanCommand("SAVE_CONFIG");
        }
        mService->BroadcastConfiguredStations(mStationsConfig);
        mSupplicantRestartCount = 0;
        // Set country code if available
        // setFrequencyBand();
        // setNetworkDetailedState(DISCONNECTED);
        doWifiBooleanCommand("AP_SCAN 1");  // CONNECT_MODE
        doWifiBooleanCommand("RECONNECT");
        transitionTo(DISCONNECTED_STATE);
        break;
        }
    case SUP_SCAN_RESULTS_EVENT: {
        Mutex::Autolock _l(mReadLock);
        mScanResultIsPending = false;
        /* bssid / frequency / signal level / flags / ssid
           00:19:e3:33:55:2e2457-64[WPA-PSK-TKIP][WPA2-PSK-TKIP+CCMP][ESS]ADTC
           00:24:d2:92:7e:e42412-65[WEP][ESS]5YPKM
           00:26:62:50:7e:4a2462-89[WEP][ESS]HD253 */
        String8 data = doWifiStringCommand("SCAN_RESULTS");
        Vector<ScannedStation>     mStations;
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
        break;
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
                break;
        } else {   // Adding a new station
            for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
                if (mStationsConfig[i].ssid == cs.ssid) {
                    SLOGW("WifiStateMachine::addOrUpdate: Attempting to add ssid=%s"
                    " but that station already exists\n", cs.ssid.string());
                    goto caseover;
                }
            }
            String8 s = doWifiStringCommand("ADD_NETWORK");
            if (s.isEmpty() || !isdigit(s.string()[0])) {
                SLOGW("WifiStateMachine::addOrUpdate: Failed to add a network [%s]\n", s.string());
                break;
            }
            network_id = atoi(s.string());
        }
        // ****************************************
        // TODO:  The preshared key and the SSID need to be properly string-escaped
        // ****************************************
        // Configure the SSID, Key management, Pre-shared key, Priority
        if ((!doWifiBooleanCommand("SET_NETWORK %d ssid \"%s\"", network_id, cs.ssid.string())
         || (!cs.key_mgmt.isEmpty()
             && !doWifiBooleanCommand("SET_NETWORK %d key_mgmt %s", network_id, cs.key_mgmt.string()))
         || (!cs.pre_shared_key.isEmpty() && cs.pre_shared_key != "*"
             && !doWifiBooleanCommand("SET_NETWORK %d psk \"%s\"", network_id, cs.pre_shared_key.string()))
         || !doWifiBooleanCommand("SET_NETWORK %d priority %d", network_id, cs.priority))
         && cs.network_id == -1) {  // We were adding a new station
            doWifiBooleanCommand("REMOVE_NETWORK %d", network_id);
            break;
        }
        // Save the configuration
        doWifiBooleanCommand("AP_SCAN 1");
        doWifiBooleanCommand("SAVE_CONFIG");
        // We need to re-read the configuration back from the supplicant
        // to correctly update the values that will be displayed to the client.
        if (index == -1) {
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
            break;
        int index = findIndexByNetworkId(network_id);
        if (index < 0)
            break;
        int p = 0;
        for (size_t i = 0 ; i < mStationsConfig.size() ; i++) {
            if (mStationsConfig[i].priority > p)
                p = mStationsConfig[i].priority;
        }
        if (mStationsConfig[index].priority < p) {
            p++;
            if (!doWifiBooleanCommand("SET_NETWORK %d priority %d", network_id, p))
                break;
            mStationsConfig.editItemAt(index).priority = p;
        }
        }
        /* fall through */
    case CMD_ENABLE_NETWORK:
        setStatus((message->command() != CMD_ENABLE_NETWORK || message->arg2() != 0)
             ? "SELECT_NETWORK %d" : "ENABLE_NETWORK %d", network_id, ConfiguredStation::ENABLED);
        result = SM_HANDLED;
        break;
    case CMD_DISABLE_NETWORK:
        setStatus("DISABLE_NETWORK %d", network_id, ConfiguredStation::DISABLED);
        result = SM_HANDLED;
        break;
    case CMD_REMOVE_NETWORK:
        setStatus("REMOVE_NETWORK %d", network_id, ConfiguredStation::CURRENT);
        result = SM_HANDLED;
        break;
    case CMD_CONNECT_NETWORK: {
        int network_id = message->arg1();
        SLOGV("TODO: WifiStateMachine.Connect_Mode_process(network_id=%d);\n", network_id);
        }
        result = SM_HANDLED;
        break;
    case CMD_START_SUPPLICANT:
        ncommand("softap fwreload %s STA", mInterface.string());
        setInterfaceState(0);
        if (request_wifi(WIFI_START_SUPPLICANT)) {
            transitionTo(DRIVER_UNLOADING_STATE);
            break;
        }
        androidCreateThread(monitorThread, this);
        break;
    }
caseover:;
    int first = 1;
    while (state && result == SM_NOT_HANDLED) {
        PROCESS_PROTO fn = mStateMap[state].mProcess;
        if (!first)
            SLOGV("......Processing message %s (%d) in state %s\n", msgStr(message->command()),
                message->command(), state_table[state].name);
        first = 0;
        if (fn) {
            result = (static_cast<WifiStateMachineActions *>(this)->*static_cast<WPROCESS_PROTO>(fn))(message);
            if (result == SM_DEFAULT) {
                STATE_TRANSITION *t = state_table[state].tran;
                result = SM_NOT_HANDLED;
                while (t && t->state) {
                    if (t->event == message->command()) {
                        result = SM_DEFER;
                        if (t->state != DEFER_STATE) {
                            transitionTo(t->state);
                            result = SM_HANDLED;
                        }
                        break;
                    }
                    t++;
                }
            }
        }
        state = mStateMap[state].mParent;
    }
    if (result == SM_NOT_HANDLED) {
        switch (message->command()) {
        case SUP_SCAN_RESULTS_EVENT:
        case CMD_LOAD_DRIVER: case CMD_UNLOAD_DRIVER:
        case CMD_START_SUPPLICANT: case CMD_STOP_SUPPLICANT:
        case SUP_CONNECTION_EVENT: case SUP_DISCONNECTION_EVENT:
        case CMD_START_SCAN: case CMD_ENABLE_BACKGROUND_SCAN:
            return SM_HANDLED;
        }
    }
    return result;
}

};  // namespace android
