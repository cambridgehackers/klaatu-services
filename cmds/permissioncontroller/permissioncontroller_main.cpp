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

  Act as an Android permission service, which allows anyone to 
  make connections to various services.
 */

#define LOG_TAG "PermController"

#include <binder/BinderService.h>
#include <binder/IPermissionController.h>

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

class PermissionController : public BinderService<PermissionController>,
    public BnPermissionController
{
    friend class BinderService<PermissionController>;
public:
    static const char *getServiceName() { return "permission"; }
    // BnPermissionController
    bool checkPermission(const String16& permission, int32_t pid, int32_t uid) {
	ALOGI("Checking permission '%s' for pid=%d uid=%d\n", String8(permission).string(), pid, uid);
        return true;
    }
};

};  // namespace android

int main(int argc, char **argv)
{
    android::PermissionController::publishAndJoinThreadPool();
    return 0;
}

