/*
  DhcpStateMachine 

  Based on DhcpStateMachine.java
*/

#ifndef _Dhcp_STATEMACHINE_H
#define _Dhcp_STATEMACHINE_H

#include "StateMachine.h"

namespace android {

class WifiStateMachine;

class DhcpStateMachine : public StateMachine
{
public:
    enum {
	CMD_START_DHCP,
	CMD_STOP_DHCP,
	CMD_RENEW_DHCP
	// If you add to this list, update the strings in dhcpstatemachine.cpp
    };

    DhcpStateMachine(WifiStateMachine *state_machine, const String8& interface);

    // Callbacks from the states
    void stopDhcp();
    bool runDhcp(bool renew);

protected:
    virtual const char *msgStr(int msg_id);

private:
    WifiStateMachine *mWifiStateMachine;
    String8           mInterface;
};

};  // namespace android

#endif // _Dhcp_STATEMACHINE_H
