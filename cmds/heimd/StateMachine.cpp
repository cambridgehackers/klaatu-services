/*
   State Machine logic
 */

#include "HeimdDebug.h"
#include "StateMachine.h"

namespace android {

// ---------------------------------------------------------------------------

// Subtract current from mRunTime and return the value in milliseconds

void Message::setDelay(int ms)
{
    mExecuteTime = systemTime() + ms2ns(ms);
}

// ---------------------------------------------------------------------------

StateMachine::StateMachine()
    : mCurrentState(0)
    , mTargetState(0)
{
}

StateMachine::~StateMachine()
{
}

void StateMachine::enqueue(Message *message)
{
    Mutex::Autolock _l(mLock);
    mQueuedMessages.push(message);
    mCondition.signal();
}

void StateMachine::enqueue(int command, int arg1, int arg2)
{
    enqueue(new Message(command, arg1, arg2));
}

void StateMachine::enqueueDelayed(Message *message, int ms)
{
    Mutex::Autolock _l(mLock);
    message->setDelay(ms);
    mDelayedMessages.push(message);
    mCondition.signal();
}

void StateMachine::enqueueDelayed(int command, int delay, int arg1, int arg2)
{
    enqueueDelayed(new Message(command, arg1, arg2), delay);
}

// Return true if 'a' is a parent of 'b' or if 'a' == 'b'
static bool isParentOf(State *a, State *b)
{
    if (!a)
	return true;  // The Null state is always a parent
    while (b) {
	if (b == a)
	    return true;
	b = b->parent();
    }
    return false;
}

static State * findCommonParent(State *a, State *b)
{
    if (isParentOf(a,b))
	return a;
    while (b) {
	if (isParentOf(b, a))
	    return b;
	b = b->parent();
    }
    return NULL;
}

static void callEnterChain(StateMachine *state_machine, State *parent, State *state)
{
    if (state && state != parent) {
	callEnterChain(state_machine, parent, state->parent());
	SLOGV(".............Calling enter on %s\n", state->name());
	state->enter(state_machine);
    }
}

static void callExitChain(StateMachine *state_machine, State *parent, State *state)
{
    while (state && state != parent) {
	SLOGV(".............Calling exit on %s\n", state->name());
	state->exit(state_machine);
	state = state->parent();
    }
}

bool StateMachine::threadLoop()
{
    while (!exitPending()) {
	while (mTargetState != mCurrentState) {
	    State *parent = findCommonParent(mTargetState, mCurrentState);
	    if (mCurrentState) {
		SLOGV("......Exiting state %s\n", mCurrentState->name());
		callExitChain(this, parent, mCurrentState);
	    }
	    mCurrentState = mTargetState;
	    if (mDeferedMessages.size() > 0) {
		Mutex::Autolock _l(mLock);
		mQueuedMessages.insertVectorAt(mDeferedMessages, 0);
		mDeferedMessages.clear();
	    }
	    if (mCurrentState) {
		SLOGV("......Entering state %s\n", mCurrentState->name());
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
		int ns = processDelayedLocked(); 
		if (ns > 0)
		    mCondition.waitRelative(mLock, ns);
	    }
	    else  // Unlimited wait
		mCondition.wait(mLock);
	}

	// Process the first message on the queue
	Message *message = mQueuedMessages[0];
	mQueuedMessages.removeAt(0);
	mLock.unlock();

	State *state = mCurrentState;
	stateprocess_t result = SM_NOT_HANDLED;
	const char *msg_str = msgStr(message->command());

	while (state && result == SM_NOT_HANDLED) {
	    SLOGV("......Processing message %s (%d) in state %s\n", 
		   (msg_str ? msg_str : "<UNKNOWN>"),
		   message->command(), state->name());
	    result = state->process(this, message);
	    state = state->parent();
	}

	switch (result) {
	case SM_HANDLED:
	    delete message;
	    break;
	case SM_NOT_HANDLED:
	    SLOGV("Warning!  Message %s (%d) not handled by current state %p\n", 
		   (msg_str ? msg_str : "<UNKNOWN>"),
		   message->command(), mCurrentState);
	    delete message;
	    break;
	case SM_DEFER:
	    SLOGV(".......Message %s (%d) is being defered by current state\n",
 		   (msg_str ? msg_str : "<UNKNOWN>"), message->command());
	    mDeferedMessages.push(message);
	    break;
	}
    }
    return false;
}

/* 
   Return the wait time in nanoseconds.  Also shift messages from the
   delay queue to the normal queue.  This routine only shifts the first
   message it finds; it doesn't guarantee that all messages will be shifted.
 */

nsecs_t StateMachine::processDelayedLocked()
{
    nsecs_t current = systemTime();
    nsecs_t delay = 0;

    for (size_t i = 0 ; i < mDelayedMessages.size() ; i++) {
	Message *m = mDelayedMessages[i];
	nsecs_t t = m->executeTime();
	if (t <= current) {
	    mDelayedMessages.removeAt(i);
	    mQueuedMessages.push(m);
	    return 0;
	}
	t -= current;
	if (delay == 0 || t < delay)
	    delay = t;
    }
    return delay;
}

void StateMachine::transitionTo(int key)
{
    if (key == CMD_TERMINATE)
	mTargetState = NULL;
    else if (mStateMap.indexOfKey(key) < 0)
	SLOGV("....ERROR: state %d doesn't have a value\n", key);
    else
	mTargetState = mStateMap.valueFor(key);
}

void StateMachine::add(int command, const char *name, State *state, State *parent)
{
    mStateMap.add(command, state);
    state->setName(name);
    state->setParent(parent);
}

const char * StateMachine::msgStr(int)
{
    return NULL;
}

// ---------------------------------------------------------------------------

}; // namespace android
