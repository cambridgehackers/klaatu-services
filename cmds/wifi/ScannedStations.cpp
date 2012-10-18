#include "WifiDebug.h"
#include "ScannedStations.h"
#include "StringUtils.h"
#include <wifi/IWifiClient.h>

namespace android {

/*
bssid / frequency / signal level / flags / ssid
00:19:e3:33:55:2e2457-64[WPA-PSK-TKIP][WPA2-PSK-TKIP+CCMP][ESS]ADTC
00:24:d2:92:7e:e42412-65[WEP][ESS]5YPKM
00:26:62:50:7e:4a2462-89[WEP][ESS]HD253
 */

void ScannedStations::update(const String8& data)
{
    mStations.clear();

    Vector<String8> lines = splitString(data.string(), '\n');
    for (size_t i = 1 ; i < lines.size() ; i++) {
	Vector<String8> elements = splitString(lines[i], '\t');
	if (elements.size() < 3 || elements.size() > 5) {
	    SLOGW("WifiStateMachine::handleScanResults() Illegal data: %s\n", 
		 lines[i].string());
	} else {
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
}

};  // namespace android
