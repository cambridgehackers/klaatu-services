/*
  Connect to a CommandListener daemon (e.g. netd)

  This is a simplified bit of code which expects to only be
  called by a SINGLE thread (no multiplexing of requests from
  multiple threads).  
 */

#include "HeimdDebug.h"
#include "DaemonConnector.h"

#include <cutils/sockets.h>

namespace android {

// ------------------------------------------------------------

DaemonConnector::DaemonConnector(const char *socketname)
{
    mFd = socket_local_client(socketname, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
    if (mFd < 0) {
	LOGW("Could not start DaemonConnector to socket %s due to error %d\n", 
		socketname, mFd);
	exit(1);
    }
}

void DaemonConnector::startRunning()
{
    status_t result = run("DaemonConnector", PRIORITY_NORMAL);
    // INVALID_OPERATION is returned if the thread is already running
    if (result != NO_ERROR && result != INVALID_OPERATION) {  
	LOGW("Could not start WifiMonitor thread due to error %d\n", result);
	exit(1);
    }
}

// All messages from the server should have a 3 digit numeric code followed
// by a text string.  
int DaemonConnector::extractCode(const char *buf)
{
    if (buf[0] >= '0' && buf[0] <= '9' && buf[1] >= '0' && buf[1] <= '9' &&
	buf[2] >= '0' && buf[2] <= '9' && buf[3] == ' ') 
	return ((buf[0] - '0') * 100 + (buf[1] - '0') * 10 + buf[2] - '0');
    LOGW("Unable to extract numeric code from '%s'\n", buf);
    return -1;
}

int DaemonConnector::extractCode(const String8& buf)
{
    return extractCode(buf.string());
}

Vector<String8> DaemonConnector::command(const String8& command, int *error_code)
{
    if (error_code)
	*error_code = -1;

    mLock.lock();
    mResponseQueue.clear();
    mLock.unlock();

    Vector<String8> response;
    LOGV("....DaemonConnector::command: %s\n", command.string());
    int len = ::write(mFd, command.string(), command.length() + 1);
    if (len < 0) {
	perror("Unable to write to daemon socket");
	exit(1);
    }

    bool complete = false;
    mLock.lock();
    while (!complete) {
	while (mResponseQueue.size() == 0)
	    mCondition.wait(mLock);
	const String8& data(mResponseQueue[0]);
	int code = extractCode(data.string());
	if (code > 0)
	    response.push(data);
	if (code >= 200 && code < 600) {
	    complete = true;
	    if (error_code)
		*error_code = code;
	}
	mResponseQueue.removeAt(0);
    }
    mLock.unlock();
    return response;
}

bool DaemonConnector::threadLoop()
{
    char buf[1024];
    int start = 0;

    while (1) {
	int count = ::read(mFd, buf + start, sizeof(buf) - start);
	if (count < 0) {
	    perror("Error reading daemon socket");
	    break;
	}
	count += start;
	start = 0;
	for (int i = 0 ; i < count ; i++) {
	    if (buf[i] == '\0') {
		String8 message = String8(buf + start, i - start);
		LOGV("......extracted message: %s\n", message.string());
		start = i + 1;
		int space_index = message.find(" ");
		if (space_index > 0) {
		    int code = extractCode(message.string());
		    if (code >= 600) 
			event(message);
		    else {
			Mutex::Autolock _l(mLock);
			mResponseQueue.push(message);
			mCondition.signal();
		    }
		} else {
		    LOGW("DaemonConnector: Illegal message '%s'\n", message.string());
		}
	    }
	}

	if (start < count) 
	    memcpy(buf, buf+start, count - start);
	start = count - start;
    }
    return true;
}

// ------------------------------------------------------------

}; // namespace android
