/*
  Supplicant State

  Matches pretty closely with SupplicantState.java
*/

#ifndef _SUPPLICANT_STATE_H
#define _SUPPLICANT_STATE_H

#include <utils/String8.h>

namespace android {

class SupplicantState {
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

    static const char *toString(int value) {
	static const char *sStateNames[] = {
	    "Disconnected", "Disabled", "Inactive", "Scanning", "Authenticating",
	    "Associating", "Associated", "WPA 4-way Handshake", "WPA Group Handshake", 
	    "Completed", "Dormant", "Uninitialized", "Invalid"
	};
	if (value >= DISCONNECTED && value <= INVALID)
	    return sStateNames[value];
	return "<ERROR>";
    }

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

};  // namespace android

#endif // _SUPPLICANT_STATE_H
