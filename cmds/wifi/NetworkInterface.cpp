/*
  Network Interface
 */

#include <stdio.h>
#include <ctype.h>
#include <cutils/sockets.h>
#include "WifiDebug.h"
#include "NetworkInterface.h"
#include "StringUtils.h"

namespace android {

NetworkInterface::NetworkInterface(WifiStateMachine *state_machine,
				   const String8& interface)
    : mStateMachine(state_machine) , mInterface(interface) , mSequenceNumber(0)
{
    indicationstart = 0;
    mFd = socket_local_client("netd", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
    if (mFd < 0) {
	SLOGW("Could not start connection to socket %s due to error %d\n", "netd", mFd);
	exit(1);
    }
}

bool NetworkInterface::wifiFirmwareReload(const char *mode) 
{
    int code = 0;
    ncommand(String8::format("softap fwreload %s %s", mInterface.string(), mode), &code);
    setInterfaceState(0);
    if (code < 200)
	SLOGW("NetworkInterface::wifiFirmwareReload error=%d\n", code);
    return (code >= 200);
}

bool NetworkInterface::getInterfaceConfig(InterfaceConfiguration& ifcfg) 
{
    int code = 0;
    Vector<String8> response = ncommand(String8::format("interface getcfg %s", mInterface.string()), &code);
    if (code != 213 || response.size() == 0) {
	SLOGW("...can't get interface config code=%d\n", code);
	return false;
    }
    // Rsp xx:xx:xx:xx:xx:xx yyy.yyy.yyy.yyy zzz [flag1 flag2 flag3]
    SLOGV("......parsing: %s\n", response[0].string());
    Vector<String8> elements = splitString(response[0], ' ', 5);
    if (elements.size() != 5) {
	SLOGW("....bad split of interface cmd '%s'\n", response[0].string());
	return false;
    }

    ifcfg.hwaddr = elements[1];
    ifcfg.ipaddr = elements[2];
    ifcfg.prefixLength = atoi(elements[3].string());
    ifcfg.flags = elements[4];
    SLOGV("..ifcfg hwaddr='%s' ipaddr='%s' prefixlen=%d flags='%s'\n",
	 ifcfg.hwaddr.string(), ifcfg.ipaddr.string(), ifcfg.prefixLength, ifcfg.flags.string());
    return true;
}

bool NetworkInterface::setInterfaceConfig(const InterfaceConfiguration& ifcfg) 
{
    int code = 0;
    ncommand(String8::format("interface setcfg %s %s %d %s", mInterface.string(),
	ifcfg.ipaddr.string(), ifcfg.prefixLength, ifcfg.flags.string()), &code);
    return (code >= 200);
}

bool NetworkInterface::setInterfaceState(int astate) 
{
    static const char *sname[] = {"down", "up"};
    InterfaceConfiguration ifcfg;

    if (!getInterfaceConfig(ifcfg)) {
	SLOGW("NetworkInterface::setInterfaceState(): Unable to get interface %s\n", 
	     mInterface.string());
	return false;
    }
    // SLOGV(".....flags before: %s\n", ifcfg.flags.string());
    ifcfg.flags = replaceString(ifcfg.flags, sname[1 - astate], sname[astate]);
    // SLOGV(".....flags after: %s\n", ifcfg.flags.string());
    if (!setInterfaceConfig(ifcfg)) {
	SLOGW("NetworkInterface::setInterfaceState(): Unable to set interface %s\n", 
	     mInterface.string());
	return false;
    }
    return true;
}

void NetworkInterface::setDefaultRoute(const char *gateway)
{
    ncommand(String8::format("interface route add %s default 0.0.0.0 0 %s", mInterface.string(), gateway), NULL);
}

void NetworkInterface::setDns(const char *dns1, const char *dns2) 
{
    String8 cmd = String8::format("resolver setifdns %s", mInterface.string());
    if (strlen(dns1) && !strcmp(dns1,"127.0.0.1"))
	cmd.appendFormat(" %s", dns1);
    if (strlen(dns2) && !strcmp(dns2,"127.0.0.1"))
	cmd.appendFormat(" %s", dns2);
    ncommand(cmd, NULL);
    ncommand(String8::format("resolver setifdns %s", mInterface.string()), NULL);
}

void NetworkInterface::flushDnsCache() 
{
    ncommand(String8::format("resolver flushif %s", mInterface.string()), NULL);
    ncommand(String8("resolver flushdefaultif"), NULL);
}

void NetworkInterface::clearInterfaceAddresses()
{
    ncommand(String8::format("interface clearaddrs %s", mInterface.string()), NULL);
}

static int extractSequence(const char *buf)
{
    int result = 0;
    const char *p = buf;
    while (*p && isdigit(*p))
	result = result * 10 + (*p++ - '0');
    return (p > buf) ? result : -1 ;
}

// All messages from the server should have a 3 digit numeric code followed
// by a sequence number and a text string
static int extractCode(const char *buf)
{
    if (isdigit(buf[0]) && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == ' ')
	return extractSequence(buf);
    return -1;
}

Vector<String8> NetworkInterface::ncommand(const String8& command, int *error_code)
{
    Vector<String8> response;
    int code = -1;

    Mutex::Autolock _l(mLock);
    mResponseQueue.clear();
    int seqno = mSequenceNumber++;

    String8 message = String8::format("%d %s", seqno, command.string());
    SLOGV("....NetworkInterface::message: '%s'\n", message.string());
    int len = ::write(mFd, message.string(), message.length() + 1);
    if (len < 0) {
	perror("Unable to write to daemon socket");
	exit(1);
    }
    while (code < 200 || code >= 600) {
	while (mResponseQueue.size() == 0)
	    mCondition.wait(mLock);
	SLOGV("....NetworkInterface::response string '%s'", mResponseQueue[0].string());
	const String8& data(mResponseQueue[0]);
	code = extractCode(data.string());
	if (code < 0) 
	    SLOGE("Failed to extract valid code from '%s'", mResponseQueue[0].string());
	else {
	    int s = extractSequence(data.string() + 4);
	    if (s < 0)
		SLOGE("Failed to extract valid sequence from '%s'", mResponseQueue[0].string());
	    else if (s != seqno)
		SLOGE("Sequence mismatch %d (should be %d)", s, seqno);
	    else {
		SLOGV(".....NetworkInterface::response: %d", code);
		if (code > 0)
		    response.push(data);
	    }
	}
	mResponseQueue.removeAt(0);
    }
    if (error_code)
	*error_code = code;
    SLOGV("....NetworkInterface::message: '%s' error_code %d\n", message.string(), code);
    return response;
}

bool NetworkInterface::process_indication()
{
    int count = ::read(mFd, indication_buf + indicationstart, sizeof(indication_buf) - indicationstart);
    if (count < 0) {
        perror("Error reading daemon socket");
        return true;
    }
    count += indicationstart;
    indicationstart = 0;
    for (int i = 0 ; i < count ; i++) {
        if (indication_buf[i] == '\0') {
        String8 message = String8(indication_buf + indicationstart, i - indicationstart);
        SLOGV("......extracted message: %s\n", message.string());
        indicationstart = i + 1;
        int space_index = message.find(" ");
        if (space_index > 0) {
            int code = extractCode(message.string());
            if (code >= 600) 
                        SLOGV("....network message=%s\n", message.string());
            else {
            Mutex::Autolock _l(mLock);
            mResponseQueue.push(message);
            mCondition.signal();
            }
        } else {
            SLOGW("NetworkInterface: Illegal message '%s'\n", message.string());
        }
        }
    }
    if (indicationstart < count) 
        memcpy(indication_buf, indication_buf+indicationstart, count - indicationstart);
    indicationstart = count - indicationstart;
    return false;
}

bool NetworkInterface::threadLoop()
{
    while (!process_indication())
        ;
    return true;
}

};  // namespace android
