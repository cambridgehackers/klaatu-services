
/*
   State Machine logic
 */

#include <sys/socket.h>
#include "WifiDebug.h"
#include "StateMachine.h"
#define FSM_INITIALIZE_CODE
#include "WifiStateMachine.h"

namespace android {

StateMachine::StateMachine() : mCurrentState(0), mTargetState(0)
{
    extraFd = -1;
    mStateMap = (State *)malloc(STATE_MAX * sizeof(State));
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xsockets) < 0) {
        SLOGV("opening stream socket pair\n");
        exit(1);
    }
}

void StateMachine::enqueue(Message *message)
{
    if (write(xsockets[1], &message, sizeof(message)) < 0)
        SLOGV("writing stream message");
}

void StateMachine::enqueueDelayed(int command, int delay)
{
    Message *message = new Message(command);
    message->mExecuteTime = systemTime() + ms2ns(delay);
    mDelayedMessages.push(message);
}

// Return true if 'a' is a parent of 'b' or if 'a' == 'b'
static bool isParentOf(State *mStateMap, int a, int b)
{
    if (!a)
	return true;  // The Null state is always a parent
    while (b) {
	if (b == a)
	    return true;
	b = mStateMap[b].mParent;
    }
    return false;
}

static void callEnterChain(StateMachine *state_machine, int parent, int state)
{
    if (state && state != parent) {
	callEnterChain(state_machine, parent, state_machine->mStateMap[state].mParent);
        if (state_machine->mStateMap[state].mEnter) {
	    SLOGV(".............Calling enter on %s\n", state_table[state].name);
            state_machine->invoke_enter(state_machine->mStateMap[state].mEnter);
        }
    }
}

void StateMachine::transitionTo(int key)
{
    if (key == CMD_TERMINATE) {
	mTargetState = 0;
	return;
    }
    mTargetState = key;
    if (!mTargetState)
	SLOGV("....ERROR: state %d doesn't have a value\n", key);
}

bool StateMachine::threadLoop()
{
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    initstates();
    while (!exitPending()) {
        Message *message = NULL;
	if (mTargetState != mCurrentState) {
	    int parent = mCurrentState;
            if (isParentOf(mStateMap, mTargetState,parent))
	        parent = mTargetState;
            else while (parent && !isParentOf(mStateMap, parent, mTargetState)) {
	    	parent = mStateMap[parent].mParent;
            }
	    if (mCurrentState) {
		SLOGV("......Exiting state %s\n", state_table[mCurrentState].name);
		int state = mCurrentState;
		while (state && state != parent) {
                    if (mStateMap[state].mExit) {
		        SLOGV(".............Calling exit on %s\n", state_table[state].name);
                        invoke_enter(mStateMap[state].mExit);
                    }
	    	    state = mStateMap[state].mParent;
		}
	    }
	    mCurrentState = mTargetState;
	    while (mDeferedMessages.size() > 0) {
 	        Message *m = mDeferedMessages[0];
 		mDeferedMessages.removeAt(0);
                enqueue(m);
	    }
	    if (mCurrentState) {
		SLOGV("......Entering state %s\n", state_table[mCurrentState].name);
		callEnterChain(this, parent, mCurrentState);
	    }
	    else {
		SLOGV("......Terminating state machine\n");
		return false;
	    }
	}
	
        while (!message) {
            int nfd = xsockets[0] + 1;
            FD_SET(xsockets[0], &readfds);
            if (extraFd != -1) {
                FD_SET(extraFd, &readfds);
                if (extraFd >= nfd)
                    nfd = extraFd + 1;
            }
            tv.tv_sec = 2;
            tv.tv_usec = 100000;
            int rv = select(nfd, &readfds, NULL, NULL, &tv);
            //SLOGV("after Select %d errno %d isset %d\n", rv, errno, FD_ISSET(xsockets[0], &readfds));
            if (rv == -1)
                SLOGV("error in select select"); // error occurred in select()
            else if (rv == 0 && mDelayedMessages.size()
             &&  mDelayedMessages[0]->mExecuteTime < systemTime()) {
                message = mDelayedMessages[0];
                mDelayedMessages.removeAt(0);
            } else if (rv > 0 && FD_ISSET(xsockets[0], &readfds)) {
                if (read(xsockets[0], &message, sizeof(message)) < 0)
                    SLOGV("error reading stream message\n");
            } else if (rv > 0 && extraFd != -1 && FD_ISSET(extraFd, &readfds))
                extraCb();
        }

	const char *msg_str = msgStr(message->command());
	switch (invoke_process(mCurrentState, message, state_table)) {
	case SM_DEFER:
	    SLOGV(".......Message %s (%d) is being defered by current state\n", msg_str, message->command());
	    mDeferedMessages.push(message);
	    break;
	default:
	    SLOGV("Warning!  Message %s (%d) not handled by current state %x\n", 
		   msg_str, message->command(), mCurrentState);
	case SM_HANDLED:
	    delete message;
	    break;
	}
    }
    return false;
}

}; // namespace android
