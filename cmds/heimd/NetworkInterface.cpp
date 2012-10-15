/*
  Network Interface
 */

#include "HeimdDebug.h"
//#include "WifiStateMachine.h"
#include "NetworkInterface.h"
#include "StringUtils.h"

#include <stdio.h>
//#include <cutils/properties.h>
//#include <hardware_legacy/wifi.h>
//#include <netutils/dhcp.h>

// ------------------------------------------------------------

namespace android {

NetworkInterface::NetworkInterface(WifiStateMachine *state_machine,
				   const String8& interface)
    : DaemonConnector("netd")
    , mStateMachine(state_machine)
    , mInterface(interface) 
{
}

void NetworkInterface::event(const String8& message) 
{
    LOGV("....network message=%s\n", message.string());
}

bool NetworkInterface::wifiFirmwareReload(const char *mode) 
{
    int code = 0;
    command(String8::format("softap fwreload %s %s", mInterface.string(), mode), &code);
    if (code < 200)
	LOGW("NetworkInterface::wifiFirmwareReload error=%d\n", code);
    return (code >= 200);
}

bool NetworkInterface::getInterfaceConfig(InterfaceConfiguration& ifcfg) 
{
    int code = 0;
    Vector<String8> response = command(String8::format("interface getcfg %s", 
						       mInterface.string()), &code);
    if (code != 213 || response.size() == 0) {
	LOGW("...can't get interface config code=%d\n", code);
	return false;
    }

    // Rsp xx:xx:xx:xx:xx:xx yyy.yyy.yyy.yyy zzz [flag1 flag2 flag3]
    LOGV("......parsing: %s\n", response[0].string());

    Vector<String8> elements = splitString(response[0], ' ', 5);
    if (elements.size() != 5) {
	LOGW("....bad split of interface cmd '%s'\n", response[0].string());
	return false;
    }

    ifcfg.hwaddr = elements[1];
    ifcfg.ipaddr = elements[2];
    ifcfg.prefixLength = atoi(elements[3].string());
    ifcfg.flags = elements[4];
    LOGV("..ifcfg hwaddr='%s' ipaddr='%s' prefixlen=%d flags='%s'\n",
	 ifcfg.hwaddr.string(), ifcfg.ipaddr.string(), ifcfg.prefixLength, ifcfg.flags.string());
    return true;
}

bool NetworkInterface::setInterfaceConfig(const InterfaceConfiguration& ifcfg) 
{
    int code = 0;
    command(String8::format("interface setcfg %s %s %d %s", mInterface.string(),
			    ifcfg.ipaddr.string(), ifcfg.prefixLength,
			    ifcfg.flags.string()), &code);
    return (code >= 200);
}

bool NetworkInterface::setInterfaceDown() 
{
    InterfaceConfiguration ifcfg;
    if (!getInterfaceConfig(ifcfg)) {
	LOGW("NetworkInterface::setInterfaceDown(): Unable to get interface %s\n", 
	     mInterface.string());
	return false;
    }
    // LOGV(".....flags before: %s\n", ifcfg.flags.string());
    ifcfg.flags = replaceString(ifcfg.flags, "up", "down");
    // LOGV(".....flags after: %s\n", ifcfg.flags.string());
    if (!setInterfaceConfig(ifcfg)) {
	LOGW("NetworkInterface::setInterfaceDown(): Unable to set interface %s\n", 
	     mInterface.string());
	return false;
    }
    return true;
}

bool NetworkInterface::setInterfaceUp() 
{
    InterfaceConfiguration ifcfg;
    if (!getInterfaceConfig(ifcfg)) {
	LOGW("NetworkInterface::setInterfaceUp(): Unable to get interface %s\n", 
	     mInterface.string());
	return false;
    }
    // LOGV(".....flags before: %s\n", ifcfg.flags.string());
    ifcfg.flags = replaceString(ifcfg.flags, "down", "up");
    //LOGV(".....flags after: %s\n", ifcfg.flags.string());
    if (!setInterfaceConfig(ifcfg)) {
	LOGW("NetworkInterface::setInterfaceUp(): Unable to set interface %s\n", 
	     mInterface.string());
	return false;
    }
    return true;
}

void NetworkInterface::setDns(const char *dns1, const char *dns2) 
{
    int code = 0;
    String8 cmd = String8::format("resolver setifdns %s", mInterface.string());
    if (strlen(dns1) && !strcmp(dns1,"127.0.0.1"))
	cmd.appendFormat(" %s", dns1);
    if (strlen(dns2) && !strcmp(dns2,"127.0.0.1"))
	cmd.appendFormat(" %s", dns2);
    command(cmd, &code);
    printf("Called '%s', result=%d\n", cmd.string(), code);

    cmd = String8::format("resolver setifdns %s", mInterface.string());
    command(cmd, &code);
    printf("Called '%s', result=%d\n", cmd.string(), code);
}

void NetworkInterface::flushDnsCache() 
{
    int code = 0;
    String8 cmd = String8::format("resolver flushif %s", mInterface.string());
    command(cmd, &code);
    printf("Called '%s', result=%d\n", cmd.string(), code);

    cmd = String8("resolver flushdefaultif");
    command(cmd, &code);
    printf("Called '%s', result=%d\n", cmd.string(), code);
}

void NetworkInterface::clearInterfaceAddresses()
{
    int code = 0;
    String8 cmd = String8::format("interface clearaddrs %s", mInterface.string());
    command(cmd, &code);
    printf("Called '%s', result=%d\n", cmd.string(), code);
}


};  // namespace android

