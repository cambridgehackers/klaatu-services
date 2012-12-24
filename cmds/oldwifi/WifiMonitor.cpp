/*
  Wifi state machine and interface.
  This object should be placed in its own thread

  This is strongly based on the WifiStateMachine.java code
  in Android.
 */

#include "WifiMonitor.h"
#include "WifiStateMachine.h"
#include "StringUtils.h"

#include <hardware_legacy/wifi.h>

// ------------------------------------------------------------

namespace android {

// ------------------------------------------------------------
// The WifiMonitor watches for supplicant messages about wifi
// state and posts them to the state machine.  It runs in its 
// own thread

const int BUF_SIZE=256;

WifiMonitor::WifiMonitor(StateMachine *state_machine, const String8& interface) 
    : mStateMachine(state_machine) 
    , mInterface(interface)
{
}

static int monitorThread(void *arg)
{
    WifiMonitor *monitor = static_cast<WifiMonitor *>(arg);
    return monitor->monitorThreadFunction();
}

void WifiMonitor::startRunning() 
{
    androidCreateThread(monitorThread, this);
}

static bool connectToSupplicant(const char *interface)
{
    int i = 0;
    while (1) {
	if (::wifi_connect_to_supplicant(interface) ==0)
	    return true;
	i++;
	if (i > 5)
	    return false;
	usleep(250 * 1000);  // Sleep for 250 ms
    }
}

int WifiMonitor::monitorThreadFunction()
{
    SLOGV("........#### Starting monitor thread ####\n");
    if (connectToSupplicant(mInterface.string())) 
	mStateMachine->enqueue(WifiStateMachine::SUP_CONNECTION_EVENT);
    else {
	mStateMachine->enqueue(WifiStateMachine::SUP_DISCONNECTION_EVENT);
	return -1;
    }
	
    while (1) {
	char buf[BUF_SIZE];  // Will store a UTF string
	int nread = ::wifi_wait_for_event(mInterface.string(), buf, BUF_SIZE);
	if (nread > 0) {
	    SLOGV("##### %s\n", buf);
	    if (strncmp(buf, "CTRL-EVENT-", 11) == 0) {
		if (handleSupplicantEvent(buf + 11))
		    return 0;
	    }
	    else if (strncmp(buf, "WPA:", 4) == 0) {
		mStateMachine->enqueue(WifiStateMachine::AUTHENTICATION_FAILURE_EVENT);
	    }
	    else 
		SLOGV(".....Unprocessed supplicant event: %s\n", buf);
	}
    }

    return 0;
}

// Return "true" if this thread should terminate

bool WifiMonitor::handleSupplicantEvent(const char *buf) 
{
    // This might be better in a hash table, but it's not too long as of yet
    if (strncmp(buf, "CONNECTED ", 10) == 0)
	handleConnected(buf + 10);
    else if (strncmp(buf, "DISCONNECTED ", 13) == 0)
	mStateMachine->enqueue(WifiStateMachine::NETWORK_DISCONNECTION_EVENT);
    else if (strncmp(buf, "STATE-CHANGE ", 13) == 0)
	handleStateChange(buf + 13);
    else if (strncmp(buf, "SCAN-RESULTS ", 13) == 0)
	mStateMachine->enqueue(WifiStateMachine::SUP_SCAN_RESULTS_EVENT);
    else if (strncmp(buf, "LINK-SPEED ", 11) == 0)
	SLOGV("....link-speed %s\n", buf + 11);
    else if (strncmp(buf, "TERMINATING ", 12) == 0) {
	SLOGV("....terminating %s\n", buf + 12);
	mStateMachine->enqueue(WifiStateMachine::SUP_DISCONNECTION_EVENT);
	return true;
    }
    else if (strncmp(buf, "DRIVER-STATE ", 13) == 0)
	SLOGV("....driver-state %s\n", buf + 13);
    else if (strncmp(buf, "EAP-FAILURE ", 12) == 0)
	SLOGV("....eap-failure %s\n", buf + 12);
    else if (strncmp(buf, "BSS-ADDED ", 10) == 0) {}
    // SLOGV(".....bss-added %s\n", buf + 10);
    else if (strncmp(buf, "BSS-REMOVED ", 12) == 0) {}
    // SLOGV(".....bss-removed %s\n", buf + 12);
    else
	SLOGV(".....unrecognized CTRL-EVENT-%s\n", buf);
    return false;
}

void WifiMonitor::handleStateChange(const char *buf) 
{
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

    if (new_state != -1)
	mStateMachine->enqueue(new Message(WifiStateMachine::SUP_STATE_CHANGE_EVENT, 
					   network_id, new_state, bssid));
}

/**
 * Regex pattern for extracting an Ethernet-style MAC address from a string.
 * Matches a strings like the following:<pre>
 * CTRL-EVENT-CONNECTED - Connection to 00:1e:58:ec:d5:6d completed (reauth) [id=1 id_str=]</pre>
 */

void WifiMonitor::handleConnected(const char *buf) 
{
    // We make a LOT of assumptions about the CONNECTED format
    if (strncmp(buf, "- Connection to ", 16) != 0) {
	SLOGV("......unrecognized connection command %s\n", buf);
	return;
    }
	
    buf += 16;
    const char *p = buf;
    while (*p != ' ' && *p != '\0')
	p++;
    if (*p == '\0') {
	SLOGV("......unable to find termination of mac address %s\n", buf);
	return;
    }

    String8 macaddr = String8(buf, p - buf);
    buf = p;
    while (*buf != '=' && *buf != '\0')
	buf++;
    if (*buf == '\0') {
	SLOGV(".....unable to find id= in %s\n", buf);
	return;
    }

    p = ++buf;
    while (*p >= '0' && *p <= '9')
	p++;
    if (*p != ' ') {  //Normally ends with a space
	SLOGV(".....unable to find termination of id in %s\n", buf);
	return;
    }

    String8 idstr = String8(buf, p-buf);
    int network_id = atoi(idstr.string());
    mStateMachine->enqueue(new Message(WifiStateMachine::NETWORK_CONNECTION_EVENT, 
				       network_id, 0, macaddr));
}


}; // namespace android


