/*
  The logic in this state machine closely follows StateMachine.java, only
  coded in C++ so we can re-use it more effectively.
 */

#ifndef _STATEMACHINE_H
#define _STATEMACHINE_H

#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <utils/threads.h>

namespace android {
class StateMachine;
class Message {
public:
    Message(int command, int arg1 = -1, int arg2 = -1, const String8& str=String8()) 
	: mCommand(command), mArg1(arg1), mArg2(arg2), mString(str) {}
    virtual ~Message() {}
    int command() const { return mCommand; }
    int arg1() const { return mArg1; }
    int arg2() const { return mArg2; }
    const String8& string() const { return mString; }
    nsecs_t        mExecuteTime;  // Only for delayed messages
private:
    int            mCommand;
    int            mArg1, mArg2;
    String8        mString;
};

enum stateprocess_t { SM_DEFAULT, SM_HANDLED, SM_NOT_HANDLED, SM_DEFER };
typedef void (StateMachine::*ENTER_EXIT_PROTO)(void);
typedef stateprocess_t (StateMachine::*PROCESS_PROTO)(Message *);
class State {
public:
    ENTER_EXIT_PROTO mEnter;
    PROCESS_PROTO    mProcess;
    ENTER_EXIT_PROTO mExit;
    int              mParent;
    const char *     mName;
};

class StateMachine : public Thread {
public:
    enum { CMD_TERMINATE = -1 };
    StateMachine();
    virtual ~StateMachine() {}
    void transitionTo(int);
    void enqueue(Message *);
    void enqueue(int command) { enqueue(new Message(command)); }
    void enqueueDelayed(int command, int delay);
    State * mStateMap;
    virtual void invoke_enter(ENTER_EXIT_PROTO) = 0;
    virtual stateprocess_t invoke_process(PROCESS_PROTO, Message *) = 0;
protected:
    virtual const char *msgStr(int msg_id) { return ""; }
    int               extraFd;
    void              (*extraCb)(void);
private:
    virtual bool      threadLoop();
    int               mCurrentState;
    int               mTargetState;
    int               xsockets[2];
    Vector<Message *> mDeferedMessages;
    Vector<Message *> mDelayedMessages;
};
}; // namespace android

#endif // _STATEMACHINE_H
