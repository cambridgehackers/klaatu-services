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
 */
/*
 * Derived from code with:
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "PowerManager"

#include <stdio.h>
#include <binder/BinderService.h>
#include <powermanager/IPowerManager.h>

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
class BnPowerManager : public BnInterface<IPowerManager>
{
public:
    virtual status_t onTransact(uint32_t code, const Parcel& data,
                                Parcel* reply, uint32_t flags = 0);
    // Must be kept in sync with frameworks/base/services/powermanager/IPowerManager.cpp
    enum {
        ACQUIRE_WAKE_LOCK = IBinder::FIRST_CALL_TRANSACTION,
        RELEASE_WAKE_LOCK = IBinder::FIRST_CALL_TRANSACTION + 4,
    };
};

class WakeLockClient
{
public:
    WakeLockClient(int flags, const sp<IBinder>& client, const String16& tag, int uid, int pid)
        : mFlags(flags), mClient(client), mTag(tag), mUid(uid), mPid(pid) {
        ALOGI("Creating new WakeLockClient flags=%d tag=%s uid=%d pid=%d\n",
              mFlags, String8(mTag).string(), mUid, mPid);
    }
    ~WakeLockClient() {
        ALOGI("Deleting WakeLockClient flags=%d uid=%d pid=%d tag=%s\n",
              mFlags, mUid, mPid, String8(mTag).string());
    }
    int          mFlags;
    sp<IBinder>  mClient;
    String16     mTag;
    int          mUid;
    int          mPid;
};

class FakePowerManager : public BinderService<FakePowerManager>,
    public BnPowerManager, public IBinder::DeathRecipient
{
    friend class BinderService<FakePowerManager>;
public:
    FakePowerManager() { }
    virtual ~FakePowerManager() { }
    static const char *getServiceName() { return "power"; }

    // IBinder::DeathRecipient
    virtual void binderDied(const wp<IBinder>& who) {
        Mutex::Autolock _l(mLock);
        releaseWakeLockLocked(who, 0, true);
    }

    // BnPowerManager
    status_t acquireWakeLock(int flags, const sp<IBinder>& lock, const String16& tag
#if (SHORT_PLATFORM_VERSION >= 44)
, const String16& packageName
#endif
);
#if (SHORT_PLATFORM_VERSION >= 44)
    status_t acquireWakeLockWithUid(int flags, const sp<IBinder>& lock, const String16& tag, const String16& packageName, int uid);
    status_t updateWakeLockUids(const sp<IBinder>& lock, int len, const int *uids);
#endif
    status_t releaseWakeLock(const sp<IBinder>& lock, int flags);

private:
    void acquireWakeLockLocked(int flags, const sp<IBinder>& lock, int uid, int pid, const String16& tag);
    void releaseWakeLockLocked(const wp<IBinder>& lock, int flags, bool death);
    mutable Mutex mLock;
    KeyedVector< wp<IBinder>, WakeLockClient *> mClients;
};

// See actual implementation in /frameworks/base/services/java/com/android/server/PowerManagerService.java
status_t FakePowerManager::acquireWakeLock(int flags, const sp<IBinder>& lock, const String16& tag
#if (SHORT_PLATFORM_VERSION >= 44)
, const String16& packageName
#endif
)
{
    int uid = IPCThreadState::self()->getCallingUid();
    int pid = IPCThreadState::self()->getCallingPid();
    // Here we may want to check permissions....someday...
    int64_t token = IPCThreadState::self()->clearCallingIdentity();
    {
        Mutex::Autolock _l(mLock);
        acquireWakeLockLocked(flags, lock, uid, pid, tag);
    }
    IPCThreadState::self()->restoreCallingIdentity(token);
    return NO_ERROR;
}

#if (SHORT_PLATFORM_VERSION >= 44)
status_t FakePowerManager::acquireWakeLockWithUid(int flags, const sp<IBinder>& lock, const String16& tag, const String16& packageName, int uid)
{
    return acquireWakeLock(flags, lock, tag, packageName);
}

status_t FakePowerManager::updateWakeLockUids(const sp<IBinder>& lock, int len, const int *uids)
{
    return NO_ERROR;
}
#endif

status_t FakePowerManager::releaseWakeLock(const sp<IBinder>& lock, int flags)
{
    int uid = IPCThreadState::self()->getCallingUid();
    // Here we may wish to enfoce permissions - make sure the same guy is releasing the wake lock...
    Mutex::Autolock _l(mLock);
    releaseWakeLockLocked(lock, flags, false);
    lock->unlinkToDeath(this);
    return NO_ERROR;
}

void FakePowerManager::acquireWakeLockLocked(int flags, const sp<IBinder>& lock, int uid, int pid, const String16& tag)
{
    ALOGI("acquireWakeLockLocked: flags=%d uid=%d pid=%d tag=%s\n",
          flags, uid, pid, String8(tag).string());
    ssize_t index = mClients.indexOfKey(lock);
    if (index < 0) {
        WakeLockClient *client = new WakeLockClient(flags, lock, tag, uid, pid);
        // Deleted a bunch of code to set minState for screen here..
        mClients.add(lock, client);
    }
    // Deleted a check for isScreenLock and PARTIAL_WAKE_LOCK
    // There is a bunc of original code that actually grabs wakelocks
    // Deleted some code that eventually talks to BatteryStats
}

void FakePowerManager::releaseWakeLockLocked(const wp<IBinder>& lock, int flags, bool death)
{
    ALOGI("releaseWakeLockLocked: flags=%d death=%d\n", flags, death);
    WakeLockClient *client = mClients.valueFor(lock);
    mClients.removeItem(lock);
    if (!client)
        return;
    // Deleted code to actually release power state and the underlying wakelocks
    delete client;
}

status_t BnPowerManager::onTransact( uint32_t code, const Parcel& data, 
    Parcel* reply, uint32_t flags )
{
    switch(code) {
    case ACQUIRE_WAKE_LOCK: {
        CHECK_INTERFACE(IPowerManager, data, reply);
        int32_t flags    = data.readInt32();
        sp<IBinder> lock = data.readStrongBinder();
        String16 tag     = data.readString16();
        return acquireWakeLock(flags, lock, tag
#if (SHORT_PLATFORM_VERSION == 44)
        ,String16("generic")
#endif
        );
        }
        break;
    case RELEASE_WAKE_LOCK: {
        CHECK_INTERFACE(IPowerManager, data, reply);
        sp<IBinder> lock = data.readStrongBinder();
        int32_t flags    = data.readInt32();
        return releaseWakeLock(lock, flags);
        }
        break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

};  // namespace android

int main(int argc, char **argv)
{
    android::FakePowerManager::publishAndJoinThreadPool();
    return 0;
}
