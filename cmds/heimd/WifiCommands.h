/*
   Some basic Wifi commands for talking with the supplicant
 */

#ifndef _WIFI_COMMANDS_H
#define _WIFI_COMMANDS_H

// Copied from android_net_wifi_Wifi.cpp
#include <stdio.h>
#include <utils/String8.h>

namespace android {

bool    doWifiCommand(const char *cmd, char *replybuf, int replybuflen);
String8 doWifiStringCommand(const char *fmt, ...);
bool    doWifiBooleanCommand(const char *expect, const char *fmt, ...);

};  // namespace android

#endif // _WIFI_CONFIG_STORE_H