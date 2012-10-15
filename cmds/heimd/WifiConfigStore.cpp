/*
 The WifiConfigStore contains information about the current 
 WPA configuration.  This is copied pretty directly from WifiConfigStore.java
 */

#include "WifiConfigStore.h"
#include "WifiCommands.h"
#include "StringUtils.h"

namespace android {

static String8 removeDoubleQuotes(const String8 s)
{
    if (s.size() >= 2 && s[0] == '"' && s[s.size()-1] == '"')
	return String8(s.string() + 1, s.size() - 2);
    return s;
}

static void readNetworkVariables(ConfiguredStation& station)
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


void WifiConfigStore::initialize() 
{
    mStations.clear();
    loadConfiguredNetworks();
    enableAllNetworks();
}

void WifiConfigStore::loadConfiguredNetworks() 
{
    // printf("......loadConfiguredNetworks()\n");
    String8 listStr = doWifiStringCommand("LIST_NETWORKS");

    Vector<String8> lines = splitString(listStr, '\n');
    // The first line is a header
    for (size_t i = 1 ; i < lines.size() ; i++) {
	Vector<String8> result = splitString(lines[i], '\t');

	ConfiguredStation cs;
	cs.network_id = atoi(result[0].string());
	readNetworkVariables(cs);
	cs.status     = ConfiguredStation::ENABLED;
	if (result.size() > 3) {
	    if (!strcmp(result[3].string(), "[CURRENT]"))
		cs.status = ConfiguredStation::CURRENT;
	    else if (!strcmp(result[3].string(), "[DISABLED]"))
		cs.status = ConfiguredStation::DISABLED;
	}
	mStations.push(cs);
     }
}

void WifiConfigStore::enableAllNetworks() 
{
    bool something_changed = false;
    for (size_t i = 0 ; i < mStations.size() ; i++) {
	ConfiguredStation& cs = mStations.editItemAt(i);
	if (cs.status == ConfiguredStation::DISABLED) {
	    if (doWifiBooleanCommand("OK", "ENABLE_NETWORK %d", cs.network_id)) {
		something_changed = true;
		cs.status = ConfiguredStation::ENABLED;
	    }
	}
    }

    if (something_changed) {
	doWifiBooleanCommand("OK", "AP_SCAN 1");
	doWifiBooleanCommand("OK", "SAVE_CONFIG");
    }
}

void WifiConfigStore::networkConnect(int network_id)
{
    for (size_t i = 0 ; i < mStations.size() ; i++) {
	ConfiguredStation& cs = mStations.editItemAt(i);
	if (cs.network_id == network_id) {
	    if (cs.status == ConfiguredStation::ENABLED)
		cs.status = ConfiguredStation::CURRENT;
	    else
		LOGI("WifiConfigStore::networkConnect to disabled station\n");
	    return;
	}
    }
}

void WifiConfigStore::networkDisconnect()
{
    for (size_t i = 0 ; i < mStations.size() ; i++) {
	ConfiguredStation& cs = mStations.editItemAt(i);
	if (cs.status == ConfiguredStation::CURRENT)
	    cs.status = ConfiguredStation::ENABLED;
    }
}

void WifiConfigStore::removeNetwork(int network_id)
{
    if (!doWifiBooleanCommand("OK", "REMOVE_NETWORK %d", network_id)) {
	LOGI("WifiConfigStore::removeNetwork %d failed\n", network_id);
    } else {
	bool removed_active = false;
	for (size_t i = 0 ; i < mStations.size() ; i++) {
	    const ConfiguredStation& cs = mStations.itemAt(i);
	    if (cs.network_id == network_id) {
		if (mStations.itemAt(i).status == ConfiguredStation::CURRENT)
		    removed_active = true;
		mStations.removeAt(i);
		break;
	    }
	}

	/* Enable other networks if we remove the active network */
	if (removed_active) {
	    for (size_t i = 0 ; i < mStations.size() ; i++) {
		ConfiguredStation& cs = mStations.editItemAt(i);
		if (cs.status == ConfiguredStation::DISABLED && 
		    doWifiBooleanCommand("OK", "ENABLE_NETWORK %d", cs.network_id)) {
		    cs.status = ConfiguredStation::ENABLED;
		}
	    }
	}
	
	doWifiBooleanCommand("OK", "AP_SCAN 1");
	doWifiBooleanCommand("OK", "SAVE_CONFIG");
    }
}

/*
  It's an update if the station has a valid network_id and
  the SSID values match.

  It's a new station if the network_id = -1 AND the SSID value
  does not already exist in the list.
 */

int WifiConfigStore::addOrUpdate(const ConfiguredStation& cs)
{
    int index = -1;
    int network_id = cs.network_id;

    LOGD("WifiConfigStore::addOrUpdate id=%d ssid=%s\n", network_id, cs.ssid.string());

    if (network_id != -1) {   // It's an update!
	index = findIndexByNetworkId(network_id);
	if (index == -1) {
	    LOGW("WifiConfigStore::addOrUpdate: Attempting to update network_id=%d"
		    " but that station doesn't not exist\n", cs.network_id);
	    return -1;
	}

    } else {   // Adding a new station
	index = findIndexBySsid(cs.ssid);
	if (index != -1) {
	    LOGW("WifiConfigStore::addOrUpdate: Attempting to add ssid=%s"
		    " but that station already exists\n", cs.ssid.string());
	    return -1;
	}

	String8 s = doWifiStringCommand("ADD_NETWORK");
	if (s.isEmpty() || s.string()[0] < '0' || s.string()[0] > '9') {
	    LOGW("WifiConfigStore::addOrUpdate: Failed to add a network [%s]\n", s.string());
	    return -1;
	}
	 
	network_id = atoi(s.string());
	//printf("* Adding new network id=%d\n", network_id);
    }

    bool update_okay = false;
    
    // ****************************************
    // TODO:  The preshared key and the SSID need to be properly string-escaped
    // ****************************************

    // Configure the SSID
    if (!doWifiBooleanCommand("OK", "SET_NETWORK %d ssid \"%s\"", network_id, cs.ssid.string())) {
	LOGW("Unable to set network %d ssid to '%s'\n", network_id, cs.ssid.string());
	goto end_of_update;
    }

    // Key management
    if (!cs.key_mgmt.isEmpty() &&
	!doWifiBooleanCommand("OK", "SET_NETWORK %d key_mgmt %s", network_id, cs.key_mgmt.string())) {
	LOGW("Unable to set network %d key_mgmt to '%s'\n", network_id, cs.key_mgmt.string());
	goto end_of_update;
    }

    // Pre-shared key
    if (!cs.pre_shared_key.isEmpty() && cs.pre_shared_key != "*" &&
	!doWifiBooleanCommand("OK", "SET_NETWORK %d psk \"%s\"", network_id, cs.pre_shared_key.string())) {
	LOGW("Unable to set network %d psk to '%s'\n", network_id, cs.pre_shared_key.string());
	goto end_of_update;
    }
    
    // Priority
    if (!doWifiBooleanCommand("OK", "SET_NETWORK %d priority %d", network_id, cs.priority)) {
	LOGW("Unable to set network %d priority to '%d'\n", network_id, cs.priority);
	goto end_of_update;
    }

update_okay = true;

end_of_update:
    if (!update_okay) {
	if (cs.network_id == -1) {  // We were adding a new station
	    doWifiBooleanCommand("OK", "REMOVE_NETWORK %d", network_id);
	    return -1;
	}
    }

    // Save the configuration
    doWifiBooleanCommand("OK", "AP_SCAN 1");
    doWifiBooleanCommand("OK", "SAVE_CONFIG");

    // We need to re-read the configuration back from the supplicant
    // to correctly update the values that will be displayed to the client.
    if (index == -1) {	// We've added a new station; shove one on the end of the list
	mStations.push(ConfiguredStation());
	index = mStations.size() - 1;
    }
    ConfiguredStation& station(mStations.editItemAt(index));
    station.network_id = network_id;
    if (cs.network_id == -1)
	station.status = ConfiguredStation::DISABLED;
    readNetworkVariables(station);
    return network_id;
}

/* 
   Fix this network to have the highest priority and disable all others.
   For the moment we'll not worry about too high of a priority
*/

void WifiConfigStore::selectNetwork(int network_id)
{
    LOGD("WifiConfigStore::selectNetwork %d\n", network_id);
    int index = findIndexByNetworkId(network_id);
    if (index < 0) {
	LOGW("WifiConfigStore::selectNetwork: network %d doesn't exist\n", network_id);
	return;
    }
    
    int p = 0;
    for (size_t i = 0 ; i < mStations.size() ; i++) {
	if (mStations[i].priority > p)
	    p = mStations[i].priority;
    }
    if (mStations[index].priority < p) {
	p++;
	if (!doWifiBooleanCommand("OK", "SET_NETWORK %d priority %d", network_id, p)) {
	    LOGW("WifiConfigStore::selectNetwork failed to priority %d: %d\n", network_id, p);
	    return;
	}
	mStations.editItemAt(index).priority = p;
    }

    enableNetwork(network_id, true);
}

void WifiConfigStore::enableNetwork(int network_id, bool disable_others)
{
    LOGD("WifiConfigStore::enableNetwork %d (disable_others=%d)\n", network_id, disable_others);
    bool result = doWifiBooleanCommand("OK", "%s_NETWORK %d",
				       (disable_others ? "SELECT" : "ENABLE"), network_id);

    if (!result) 
	LOGW("WifiConfigStore::enableNetwork failed for %d\n", network_id);
    else {
	for (size_t i = 0 ; i < mStations.size() ; i++) {
	    ConfiguredStation& station(mStations.editItemAt(i));
	    if (station.network_id == network_id)
		station.status = ConfiguredStation::ENABLED;
	    else if (disable_others)
		station.status = ConfiguredStation::DISABLED;
	}
    }
}

void WifiConfigStore::disableNetwork(int network_id)
{
    LOGD("WifiConfigStore::disableNetwork %d\n", network_id);
    if (!doWifiBooleanCommand("OK", "DISABLE_NETWORK %d", network_id))
	LOGW("WifiConfigStore::disableNetwork failed for %d\n", network_id);
    else {
	for (size_t i = 0 ; i < mStations.size() ; i++) {
	    ConfiguredStation& station(mStations.editItemAt(i));
	    if (station.network_id == network_id)
		station.status = ConfiguredStation::DISABLED;
	}
    }
}

int WifiConfigStore::findIndexByNetworkId(int network_id)
{
    for (size_t i = 0 ; i < mStations.size() ; i++) {
	if (mStations[i].network_id == network_id)
	    return i;
    }
    return -1;
}

int WifiConfigStore::findIndexBySsid(const String8& ssid)
{
    for (size_t i = 0 ; i < mStations.size() ; i++) {
	if (mStations[i].ssid == ssid) 
	    return i;
    }
    return -1;
}

};  // namespace android
