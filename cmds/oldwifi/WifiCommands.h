/*
   Some basic Wifi commands for talking with the supplicant
 */

#ifndef _WIFI_COMMANDS_H
#define _WIFI_COMMANDS_H

// Copied from android_net_wifi_Wifi.cpp
#include <stdio.h>
#include <utils/String8.h>

namespace android {

bool    doWifiCommand(const char *interface, const char *cmd, char *replybuf, int replybuflen);
String8 doWifiStringCommand(const char *interface, const char *fmt, ...);
bool    doWifiBooleanCommand(const char *interface, const char *expect, const char *fmt, ...);

bool    doWifiCommand(const String8& interface, const char *cmd, char *replybuf, int replybuflen);
String8 doWifiStringCommand(const String8& interface, const char *fmt, ...);
bool    doWifiBooleanCommand(const String8& interface, const char *expect, const char *fmt, ...);

};  // namespace android

#endif // _WIFI_CONFIG_STORE_H
