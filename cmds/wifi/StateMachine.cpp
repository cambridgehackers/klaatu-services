
#define NEWPIPE
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
    mStateMap = (State *)malloc(STATE_MAX * sizeof(State));
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xsockets) < 0) {
        SLOGV("opening stream socket pair\n");
        exit(1);
    }
SLOGV("socket pair fd %d:%d\n", xsockets[0], xsockets[1]);
}

void StateMachine::enqueue(Message *message)
{
    Mutex::Autolock _l(mLock);
    mQueuedMessages.push(message);
SLOGV("before write %x message %p\n", xsockets[1], message);
#ifdef NEWPIPE
        if (write(xsockets[1], &message, sizeof(message)) < 0)
            SLOGV("writing stream message");
#else
    mCondition.signal();
#endif
}

void StateMachine::enqueueDelayed(int command, int delay)
{
    Mutex::Autolock _l(mLock);
SLOGV("enqDelay %d %d\n", command, delay);
    Message *message = new Message(command);
    message->setDelay(delay);
    mDelayedMessages.push(message);
    mCondition.signal();
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
	SLOGV(".............Calling enter on %s\n", state_table[state].name);
        state_machine->invoke_enter(state_machine->mStateMap[state].mEnter);
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
		    SLOGV(".............Calling exit on %s\n", state_table[state].name);
                    invoke_enter(mStateMap[state].mExit);
	    	    state = mStateMap[state].mParent;
		}
	    }
	    mCurrentState = mTargetState;
#if 0
	    if (mDeferedMessages.size() > 0) {
		Mutex::Autolock _l(mLock);
		mQueuedMessages.insertVectorAt(mDeferedMessages, 0);
		mDeferedMessages.clear();
	    }
#else
	    while (mDeferedMessages.size() > 0) {
 	        Message *m = mDeferedMessages[0];
 		mDeferedMessages.removeAt(0);
                enqueue(m);
	    }
#endif
	    if (mCurrentState) {
		SLOGV("......Entering state %s\n", state_table[mCurrentState].name);
		callEnterChain(this, parent, mCurrentState);
	    }
	    else {
		SLOGV("......Terminating state machine\n");
		return false;
	    }
	}
	
        Message *message = NULL;
#ifdef NEWPIPE
while (1) {
    FD_SET(xsockets[0], &readfds);
    int nfd = xsockets[0] + 1;
    tv.tv_sec = 2;
    tv.tv_usec = 100000;
    //SLOGV("before Select\n");
    int rv = select(nfd, &readfds, NULL, NULL, &tv);
    SLOGV("after Select %d errno %d isset %d\n", rv, errno, FD_ISSET(xsockets[0], &readfds));
    if (rv == -1) {
        SLOGV("error in select select"); // error occurred in select()
    } else if (rv == 0) {
        //SLOGV("Timeout occurred!  %d\n", mDelayedMessages.size(), mDeferedMessages.size());
        if (mDelayedMessages.size()) {
            message = mDelayedMessages[0];
            mDelayedMessages.removeAt(0);
            //enqueue(message);
	mLock.lock();
            goto process_first;
        }
    } else {
        // one or both of the descriptors have data
        if (FD_ISSET(xsockets[0], &readfds)) {
            printf ("got data\n");
            int len = 0;
            if ((len = read(xsockets[0], &message, sizeof(message))) < 0)
                SLOGV("error reading stream message\n");
            SLOGV("after read %x len %d message %p\n", xsockets[0], len, message);
            break;
        }
    }
}
	mLock.lock();
#else
	mLock.lock();
	while (mQueuedMessages.size() == 0) {
	    if (mDelayedMessages.size()) {
                nsecs_t ns = 0;
                nsecs_t current = systemTime();
                for (size_t i = 0 ; i < mDelayedMessages.size() ; i++) {
	            message = mDelayedMessages[i];
	            nsecs_t t = message->executeTime();
	            if (t <= current) {
	                mDelayedMessages.removeAt(i);
	                //mQueuedMessages.push(message);
                        goto process_first;
	            }
	            t -= current;
	            if (ns == 0 || t < ns)
	                ns = t;
                }
		if (ns > 0)
		    mCondition.waitRelative(mLock, ns);
	    }
	    else  // Unlimited wait
		mCondition.wait(mLock);
	}
	message = mQueuedMessages[0];
	mQueuedMessages.removeAt(0);
#endif
	// Process the first message on the queue
process_first:
	mLock.unlock();

	int state = mCurrentState;
	stateprocess_t result = SM_NOT_HANDLED;
	const char *msg_str = msgStr(message->command());

	while (state && result == SM_NOT_HANDLED) {
	    SLOGV("......Processing message %s (%d) in state %s\n", msg_str,
		   message->command(), state_table[state].name);
	    STATE_TRANSITION *t = state_table[state].tran;
            result = invoke_process(mStateMap[state].mProcess, message);
	    state = mStateMap[state].mParent;
	    if (result == SM_DEFAULT) {
		result = SM_NOT_HANDLED;
		while (t && t->state) {
		    if (t->event == message->command()) {
			result = SM_DEFER;
		        if (t->state != DEFER_STATE) {
		            transitionTo(t->state);
		            result = SM_HANDLED;
			}
			break;
		    }
		    t++;
		}
	    }
	}
	switch (result) {
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
