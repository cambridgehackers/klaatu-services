/*
  Statemachine

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

// ---------------------------------------------------------------------------

class Message {
public:
    Message(int command, int arg1 = -1, int arg2 = -1, const String8& str=String8()) 
	: mCommand(command), mArg1(arg1), mArg2(arg2), mString(str) {}
    virtual ~Message() {}

    int command() const { return mCommand; }
    int arg1() const { return mArg1; }
    int arg2() const { return mArg2; }
    const String8& string() const { return mString; }

    nsecs_t executeTime() const { return mExecuteTime; }
    void    setDelay(int ms);

private:
    int            mCommand;
    int            mArg1, mArg2;
    String8        mString;
    nsecs_t        mExecuteTime;  // Only for delayed messages
};

// ---------------------------------------------------------------------------

enum stateprocess_t { SM_HANDLED, SM_NOT_HANDLED, SM_DEFER };

class State {
public:
    State() : mName("Default"), mParent(0) {}
    virtual ~State() {}
    
    virtual void           enter(StateMachine *) {}
    virtual stateprocess_t process(StateMachine *, Message *) { return SM_NOT_HANDLED; }
    virtual void           exit(StateMachine *) {}

    const char *name() const { return mName.string(); }
    void setName(const char *name) { mName = name; }

    State *parent() const { return mParent; }
    void setParent(State *parent) { mParent = parent; }

private:
    String8 mName;
    State   *mParent;
};

// ---------------------------------------------------------------------------

class StateMachine : public Thread {
public:
    enum { CMD_TERMINATE = -1 };

    StateMachine();
    virtual ~StateMachine();

    void transitionTo(int);
    void enqueue(Message *);
    void enqueue(int command, int arg1=-1, int arg2=-1); 
    void enqueueDelayed(Message *, int delay);
    void enqueueDelayed(int command, int delay, int arg1=-1, int arg2=-1);

protected:
    void add(int, const char *, State *, State *parent=0);
    virtual const char *msgStr(int msg_id);

private:
    virtual bool threadLoop();
    nsecs_t processDelayedLocked();

private:
    mutable Mutex     mLock;  // Protects mQueuedMessages
    mutable Condition mCondition;
    State            *mCurrentState;
    State            *mTargetState;

    KeyedVector<int, State *> mStateMap;
    Vector<Message *> mQueuedMessages;
    Vector<Message *> mDeferedMessages;
    Vector<Message *> mDelayedMessages;
};

// ---------------------------------------------------------------------------

}; // namespace android


#endif // _STATEMACHINE_H
