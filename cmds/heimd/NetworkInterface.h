/*
  The NetworkInterface connects to netd
*/

#ifndef _NETWORK_INTERFACE_H
#define _NETWORK_INTERFACE_H

#include "DaemonConnector.h"

namespace android {

class WifiStateMachine;

class NetworkInterface : public DaemonConnector {
public:
    struct InterfaceConfiguration {
	String8 hwaddr;
	String8 ipaddr;
	int prefixLength;
	String8 flags;
    };

    NetworkInterface(WifiStateMachine *state_machine, const String8& interface);
    virtual void event(const String8& message);

    bool wifiFirmwareReload(const char *mode);
    bool getInterfaceConfig(InterfaceConfiguration& ifcfg);
    bool setInterfaceConfig(const InterfaceConfiguration& ifcfg);
    bool setInterfaceDown();
    bool setInterfaceUp();
    void setDns(const char *dns1, const char *dns2);
    void flushDnsCache();
    void clearInterfaceAddresses();

private:
    WifiStateMachine *mStateMachine;
    String8           mInterface;
};

};  // namespace android

#endif // _NETWORK_INTERFACE_H
