
/* Original author John Ankcorn
 *
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
 */

#define LOG_TAG "InputManager"
//#define LOG_NDEBUG 0

#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
#include "ui/EventHub.h"
#include "ui/InputReader.h"
#define DISPATCH_CLASS InputDispatcherInterface
#else
#include "EventHub.h"
#include "InputReader.h"
#define DISPATCH_CLASS InputListenerInterface
#endif
#include <cutils/log.h>

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
#ifndef ALOGE /* Names changed in Android 4.1 */
#define ALOGE LOGE
#endif
#ifndef ALOGW /* Names changed in Android 4.1 */
#define ALOGW LOGW
#endif

namespace android {
class KlaatuInputListener: public DISPATCH_CLASS {
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
    void dump(String8&)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void dispatchOnce()
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    int32_t injectInputEvent(const InputEvent*, int32_t, int32_t, int32_t, int32_t)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return 0;
    }
    void setInputWindows(const Vector<InputWindow>&)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void setFocusedApplication(const InputApplication*)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void setInputDispatchMode(bool, bool)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    status_t registerInputChannel(const sp<InputChannel>&, bool)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return 0;
    }
    status_t unregisterInputChannel(const sp<InputChannel>&)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return 0;
    }
    void notifyConfigurationChanged(nsecs_t)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void notifyKey(nsecs_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, nsecs_t)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void notifyMotion(nsecs_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, const int32_t*, const PointerCoords*, float, float, nsecs_t)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void notifySwitch(nsecs_t, int32_t, int32_t, uint32_t)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
#else
    void notifyConfigurationChanged(const NotifyConfigurationChangedArgs* args)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void notifyKey(const NotifyKeyArgs* args)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void notifyMotion(const NotifyMotionArgs* args)
    {
        printf("[%s:%d] time %lld device %d action %x flag %x state %x down %lld\n", __FUNCTION__, __LINE__,
            args->eventTime, args->deviceId, args->action, args->flags, args->buttonState, args->downTime);
    }
    void notifySwitch(const NotifySwitchArgs* args)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void notifyDeviceReset(const NotifyDeviceResetArgs* args)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
#endif // not 2.3
};

class KlaatuReaderPolicy: public InputReaderPolicyInterface {
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
    bool getDisplayInfo(int32_t, int32_t*, int32_t*, int32_t*)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return true;
    }
    bool filterTouchEvents()
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return true;
    }
    bool filterJumpyTouchEvents()
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return true;
    }
    nsecs_t getVirtualKeyQuietTime()
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return 0;
    }
    void getVirtualKeyDefinitions(const String8&, Vector<VirtualKeyDefinition>&)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void getInputDeviceCalibration(const String8&, InputDeviceCalibration&)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
    void getExcludedDeviceNames(Vector<String8>&)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    }
#else
    void getReaderConfiguration(InputReaderConfiguration* outConfig)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 42)
static DisplayViewport vport;
        outConfig->setDisplayInfo(false, vport);
#else
        outConfig->setDisplayInfo(0, false, 100, 200, 0);
#endif
    }
    sp<PointerControllerInterface> obtainPointerController(int32_t deviceId)
    {
        printf("[%s:%d] %x\n", __FUNCTION__, __LINE__, deviceId);
        return NULL;
    }
    String8 getDeviceAlias(const InputDeviceIdentifier& identifier)
    {
        printf("[%s:%d] '%s'\n", __FUNCTION__, __LINE__, identifier.name.string());
        return String8("foo");
    }
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 40)
#else
    sp<KeyCharacterMap> getKeyboardLayoutOverlay(const String8& inputDeviceDescriptor)
    {
        printf("[%s:%d] '%s'\n", __FUNCTION__, __LINE__, inputDeviceDescriptor.string());
        return NULL;
    }
#endif
    void notifyInputDevicesChanged(const Vector<InputDeviceInfo>& inputDevices)
    {
        printf("[%s:%d]\n", __FUNCTION__, __LINE__);
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 40)
#else
        for (unsigned int i = 0; i < inputDevices.size(); i++)
            printf("name %s\n", inputDevices[i].getIdentifier().name.string());
#endif
    }
#endif // not 2.3
};

} // namespace android
using namespace android;

int main(int argc, char **argv)
{
    ALOGI("inputserver starting up");
    sp<KlaatuReaderPolicy> this_policy = new KlaatuReaderPolicy();
    sp<KlaatuInputListener> this_listener = new KlaatuInputListener();
    sp<InputReaderInterface> mReader = new InputReader(new EventHub(), this_policy, this_listener);
    sp<InputReaderThread> mReaderThread = new InputReaderThread(mReader);
    status_t result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    if (result) {
        ALOGE("Could not start InputReader thread due to error %d.", result);
        return result;
    }

    while(1)
       sleep(1000);
    result = mReaderThread->requestExitAndWait();
    if (result) {
        ALOGW("Could not stop InputReader thread due to error %d.", result);
    }
}
