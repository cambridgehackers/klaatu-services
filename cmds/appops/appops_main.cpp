/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.

  Act as an Android operations service, which allows anyone to 
  make connections to various services.
 */

#define LOG_TAG "AppOps"

#include <binder/BinderService.h>
#include <binder/IAppOpsService.h>

#ifndef ALOGI_IF /* Names changed in Android 4.1 */
#define ALOGI_IF LOGI_IF
#endif
#ifndef ALOGE_IF /* Names changed in Android 4.1 */
#define ALOGE_IF LOGE_IF
#endif
#ifndef ALOGD /* Names changed in Android 4.1 */
#define ALOGD LOGD
#endif
#ifndef ALOGI /* Names changed in Android 4.1 */
#define ALOGI LOGI
#endif

namespace android {

class AppOpsService : public BinderService<AppOpsService>,
    public BnAppOpsService
{
    friend class BinderService<AppOpsService>;
public:
    static const char *getServiceName() { return "appops"; }

    int32_t checkOperation(int32_t code, int32_t uid, const String16& name)
    {
	ALOGI("Checking operation '%s' for code=%d uid=%d\n",
              String8(name).string(), code, uid);
        return MODE_ALLOWED;
    }

    int32_t noteOperation(int32_t code, int32_t uid, const String16& name)
    {
	ALOGI("Noting operation '%s' for code=%d uid=%d\n",
              String8(name).string(), code, uid);
        return MODE_ALLOWED;
    }

    int32_t startOperation(
#if (SHORT_PLATFORM_VERSION >= 44)
        const sp<IBinder>& token,
#endif
        int32_t code, int32_t uid, const String16& name)
    {
	ALOGI("Starting operation '%s' for code=%d uid=%d\n",
              String8(name).string(), code, uid);
        return MODE_ALLOWED;
    }

    void finishOperation(
#if (SHORT_PLATFORM_VERSION >= 44)
        const sp<IBinder>& token,
#endif
        int32_t code, int32_t uid, const String16& name)
    {
	ALOGI("Finishing operation '%s' for code=%d uid=%d\n",
              String8(name).string(), code, uid);
    }

    void startWatchingMode(int32_t op, const String16& name, const sp<IAppOpsCallback>& cb)
    {
	ALOGI("Starting watch mode '%s' for op=%d\n",
              String8(name).string(), op);
    }

    void stopWatchingMode(const sp<IAppOpsCallback>& cb)
    {
	ALOGI("Stopping watching mode\n");
    }

#if (SHORT_PLATFORM_VERSION >= 44)
    sp<IBinder> getToken(const sp<IBinder>& clientToken)
    {
	ALOGI("getToken\n");
        return NULL;
    }
#endif
};

};  // namespace android

int main(int argc, char **argv)
{
    android::AppOpsService::publishAndJoinThreadPool();
    return 0;
}

