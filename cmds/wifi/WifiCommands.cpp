#include "WifiCommands.h"
#include <hardware_legacy/wifi.h>

namespace android {

const int BUF_SIZE=256;

bool doWifiCommand(const char *interface, const char *cmd, char *replybuf, int replybuflen)
{
    size_t reply_len = replybuflen - 1;
    if (::wifi_command(interface, cmd, replybuf, &reply_len) != 0)
	return false;

    if (reply_len > 0 && replybuf[reply_len-1] == '\n')
	replybuf[reply_len -1] = '\0';
    else
	replybuf[reply_len] = '\0';
    // printf("[[[%s]]]\n", replybuf);
    return true;
}

bool doWifiCommand(const String8& interface, const char *cmd, char *replybuf, int replybuflen)
{
    return doWifiCommand(interface.string(), cmd, replybuf, replybuflen);
}

String8 doWifiStringCommand(const char *interface, const char *fmt, va_list args)
{
    char buf[BUF_SIZE];
    int byteCount = vsnprintf(buf, sizeof(buf), fmt, args);
    if (byteCount < 0 || byteCount >= BUF_SIZE)
	return String8();
    char reply[4096];
    if (!doWifiCommand(interface, buf, reply, sizeof(reply)))
	return String8();
    return String8(reply);
}

String8 doWifiStringCommand(const char *interface, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = doWifiStringCommand(interface, fmt, args);
    va_end(args);
    return result;
}

String8 doWifiStringCommand(const String8& interface, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = doWifiStringCommand(interface.string(), fmt, args);
    va_end(args);
    return result;
}

bool doWifiBooleanCommand(const char *interface, const char *expect, const char *fmt, va_list args)
{
    char buf[BUF_SIZE];
    int byteCount = vsnprintf(buf, sizeof(buf), fmt, args);
    if (byteCount < 0 || byteCount >= BUF_SIZE)
	return false;
    char reply[4096];
    if (!doWifiCommand(interface, buf, reply, sizeof(reply)))
	return false;
    return (strcmp(reply, expect) == 0);
}

bool doWifiBooleanCommand(const char *interface, const char *expect, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool result = doWifiBooleanCommand(interface, expect, fmt, args);
    va_end(args);
    return result;
}

bool doWifiBooleanCommand(const String8& interface, const char *expect, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool result = doWifiBooleanCommand(interface.string(), expect, fmt, args);
    va_end(args);
    return result;
}

};
