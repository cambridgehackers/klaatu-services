/*
  Connect to a CommandListener daemon like netd
*/

#ifndef _DAEMON_CONNECTOR_H
#define _DAEMON_CONNECTOR_H

#include <utils/threads.h>
#include <utils/String8.h>
#include <utils/Vector.h>

namespace android {

class DaemonConnector : public Thread {
public:
    DaemonConnector(const char *socketname);

    void startRunning();

    Vector<String8> command(const String8&, int *error_code = 0);
    virtual void    event(const String8&) = 0;  // Must override in subclass

protected:
    virtual bool threadLoop();

private:
    mutable Mutex     mLock;     // Protects the response queue
    mutable Condition mCondition;
    int               mFd;
    Vector<String8>   mResponseQueue;
    int               mSequenceNumber;
};

};  // namespace android

#endif // _DAEMON_CONNECTOR
