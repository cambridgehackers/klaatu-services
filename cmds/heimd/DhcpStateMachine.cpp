/*
  DhcpStateMachine

  Runs as its own state machine because DHCP commands take a _long_ time
  to complete
 */

#include "DhcpStateMachine.h"
#include "WifiStateMachine.h"
#include <netutils/dhcp.h>
#include <cutils/properties.h>

namespace android {

enum {
    DHCP_STOPPED_STATE,
    DHCP_RUNNING_STATE
};

static const char *sMessageToString[] = {
    "CMD_START_DHCP",
    "CMD_STOP_DHCP",
    "CMD_RENEW_DHCP"
};

// ------------------------------------------------------------

class DhcpDefaultState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	DhcpStateMachine *dsm = static_cast<DhcpStateMachine *>(stateMachine);
	switch (message->command()) {
	case DhcpStateMachine::CMD_RENEW_DHCP:
	    LOGE("Failed to handle DHCP renewal");
	    break;
	default:
	    LOGD("...ERROR - unhandled message %s (%d)\n", 
		 sMessageToString[message->command()], message->command());
	    break;
	}
	return SM_HANDLED;
    }
};

class DhcpStoppedState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	DhcpStateMachine *dsm = static_cast<DhcpStateMachine *>(stateMachine);
	switch (message->command()) {
	case DhcpStateMachine::CMD_START_DHCP:
	    if (dsm->runDhcp(false))
		stateMachine->transitionTo(DHCP_RUNNING_STATE);
	    return SM_HANDLED;   
	case DhcpStateMachine::CMD_STOP_DHCP:
	    return SM_HANDLED;   // Ignore
	}
	return SM_NOT_HANDLED;
    }
};

class DhcpRunningState : public State {
public:
    stateprocess_t process(StateMachine *stateMachine, Message *message) {
	DhcpStateMachine *dsm = static_cast<DhcpStateMachine *>(stateMachine);
	switch (message->command()) {
	case DhcpStateMachine::CMD_START_DHCP:
	    return SM_HANDLED;   // Ignore
	case DhcpStateMachine::CMD_STOP_DHCP:
	    dsm->stopDhcp();
	    stateMachine->transitionTo(DHCP_STOPPED_STATE);
	    return SM_HANDLED; 
	case DhcpStateMachine::CMD_RENEW_DHCP:
	    if (!dsm->runDhcp(true))
		stateMachine->transitionTo(DHCP_STOPPED_STATE);
	    return SM_HANDLED; 
	}
	return SM_NOT_HANDLED;
    }
};

// ------------------------------------------------------------

DhcpStateMachine::DhcpStateMachine(WifiStateMachine *state_machine, const String8& interface)
    : mWifiStateMachine(state_machine)
    , mInterface(interface)
{
    ::dhcp_stop(mInterface.string());

    DhcpDefaultState *default_state = new DhcpDefaultState;
    add(DHCP_STOPPED_STATE, "DhcpStopped", new DhcpStoppedState, default_state);
    add(DHCP_RUNNING_STATE, "DhcpRunning", new DhcpRunningState, default_state);
    transitionTo(DHCP_STOPPED_STATE);

    status_t result = run("DhcpStateMachine", PRIORITY_NORMAL);
    LOG_ALWAYS_FATAL_IF(result, "Could not start DhcpStateMachine thread due to error %d\n", result);
}

const char * DhcpStateMachine::msgStr(int msg_id)
{
    return sMessageToString[msg_id];
}

void DhcpStateMachine::stopDhcp()
{
    ::dhcp_stop(mInterface.string());
}

bool DhcpStateMachine::runDhcp(bool renew)
{
    char     ipaddr[PROPERTY_VALUE_MAX];
    uint32_t prefixLength;
    char     gateway[PROPERTY_VALUE_MAX];
    char     dns1[PROPERTY_VALUE_MAX];
    char     dns2[PROPERTY_VALUE_MAX];
    char     server[PROPERTY_VALUE_MAX];
    uint32_t lease;

    int result = ::dhcp_do_request(mInterface, ipaddr, gateway, &prefixLength,
				   dns1, dns2, server, &lease);
    if (result != 0) {
	LOGD("...failed to acquire DHCP lease: %d\n", result);
	mWifiStateMachine->enqueue(WifiStateMachine::DHCP_FAILURE);
    }
    else {
	LOGD("...acquired DHCP lease: %d\n", result);
	mWifiStateMachine->enqueue(new DhcpSuccessMessage(ipaddr, gateway, dns1, dns2, server));
    }

    return (result == 0);
}


};  // namespace android
