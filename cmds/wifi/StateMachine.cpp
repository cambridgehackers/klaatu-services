/*
   State Machine logic
 */

#include "WifiDebug.h"
#include "StateMachine.h"
#define FSM_INITIALIZE_CODE
#include "WifiStateMachine.h"

namespace android {

StateMachine::StateMachine() : mCurrentState(0), mTargetState(0)
{
    mStateMap = (State *)malloc(STATE_MAX * sizeof(State));
}

void StateMachine::enqueue(Message *message)
{
    Mutex::Autolock _l(mLock);
    mQueuedMessages.push(message);
    mCondition.signal();
}

void StateMachine::enqueueDelayed(int command, int delay)
{
    Mutex::Autolock _l(mLock);
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
	    if (mDeferedMessages.size() > 0) {
		Mutex::Autolock _l(mLock);
		mQueuedMessages.insertVectorAt(mDeferedMessages, 0);
		mDeferedMessages.clear();
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
	
	mLock.lock();
	while (mQueuedMessages.size() == 0) {
	    if (mDelayedMessages.size()) {
                nsecs_t ns = 0;
                nsecs_t current = systemTime();
                for (size_t i = 0 ; i < mDelayedMessages.size() ; i++) {
	            Message *m = mDelayedMessages[i];
	            nsecs_t t = m->executeTime();
	            if (t <= current) {
	                mDelayedMessages.removeAt(i);
	                mQueuedMessages.push(m);
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
process_first:
	// Process the first message on the queue
	Message *message = mQueuedMessages[0];
	mQueuedMessages.removeAt(0);
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
