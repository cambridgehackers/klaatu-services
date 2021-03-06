
/*
   State Machine logic
 */

#include <sys/socket.h>
#include "WifiDebug.h"
#include "StateMachine.h"

namespace android {

StateMachine::StateMachine() : mCurrentState(0), mTargetState(0)
{
    extraFd = -1;
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
    while (!exitPending()) {
        Message *message = NULL;
        mCurrentState = mTargetState;
        while (mDeferedMessages.size() > 0) {
            Message *m = mDeferedMessages[0];
            mDeferedMessages.removeAt(0);
            enqueue(m);
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
        switch (invoke_process(mCurrentState, message)) {
        case SM_DEFER:
            SLOGV(".......Message %s (%d) is being defered by current state\n", msg_str, message->command());
            mDeferedMessages.push(message);
            break;
        default:
            SLOGV("Warning!  Message %s (%d) not handled by current state %d\n", 
                   msg_str, message->command(), mCurrentState); //state_table[mCurrentState].name);
        case SM_HANDLED:
            delete message;
            break;
        }
    }
    return false;
}

}; // namespace android
