#include "WifiCommands.h"
#include <hardware_legacy/wifi.h>

namespace android {

const int BUF_SIZE=256;

bool doWifiCommand(const char *cmd, char *replybuf, int replybuflen)
{
    size_t reply_len = replybuflen - 1;
    if (::wifi_command(cmd, replybuf, &reply_len) != 0)
	return false;

    if (reply_len > 0 && replybuf[reply_len-1] == '\n')
	replybuf[reply_len -1] = '\0';
    else
	replybuf[reply_len] = '\0';
    // printf("[[[%s]]]\n", replybuf);
    return true;
}

String8 doWifiStringCommand(const char *fmt, ...)
{
    char buf[BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int byteCount = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (byteCount < 0 || byteCount >= BUF_SIZE)
	return String8();
    char reply[4096];
    if (!doWifiCommand(buf, reply, sizeof(reply)))
	return String8();
    return String8(reply);
}

bool doWifiBooleanCommand(const char *expect, const char *fmt, ...)
{
    char buf[BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int byteCount = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (byteCount < 0 || byteCount >= BUF_SIZE)
	return false;
    char reply[4096];
    if (!doWifiCommand(buf, reply, sizeof(reply)))
	return false;
    return (strcmp(reply, expect) == 0);
}


};
