/*
  The NetworkInterface connects to netd
*/

#ifndef _NETWORK_INTERFACE_H
#define _NETWORK_INTERFACE_H

#include <utils/threads.h>
#include <utils/String8.h>
#include <utils/Vector.h>

namespace android {

class WifiStateMachine;

class NetworkInterface : public Thread {
public:
    struct InterfaceConfiguration {
	String8 hwaddr;
	String8 ipaddr;
	int prefixLength;
	String8 flags;
    };

    NetworkInterface(WifiStateMachine *state_machine, const String8& interface);
    bool wifiFirmwareReload(const char *mode);
    bool getInterfaceConfig(InterfaceConfiguration& ifcfg);
    bool setInterfaceConfig(const InterfaceConfiguration& ifcfg);
    bool setInterfaceState(int astate);
    void setDefaultRoute(const char *gateway);
    void setDns(const char *dns1, const char *dns2);
    void flushDnsCache();
    void clearInterfaceAddresses();
    Vector<String8> ncommand(const String8&, int *error_code = 0);

protected:
    virtual bool threadLoop();

private:
    WifiStateMachine *mStateMachine;
    String8           mInterface;
    mutable Mutex     mLock;     // Protects the response queue
    mutable Condition mCondition;
    int               mFd;
    Vector<String8>   mResponseQueue;
    int               mSequenceNumber;
};

};  // namespace android

#endif // _NETWORK_INTERFACE_H
