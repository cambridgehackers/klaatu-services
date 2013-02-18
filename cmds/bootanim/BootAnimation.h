/*
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

#ifndef ANDROID_BOOTANIMATION_H
#define ANDROID_BOOTANIMATION_H

#include <stdint.h>
#include <sys/types.h>

#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION <= 40)
#include <utils/AssetManager.h>
#else
#include <androidfw/AssetManager.h>
#endif
#include <utils/threads.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

class SkBitmap;

namespace android {

class Surface;
class SurfaceComposerClient;
class SurfaceControl;

// ---------------------------------------------------------------------------

class BootAnimation : public Thread, public IBinder::DeathRecipient
{
public:
                BootAnimation();
    virtual     ~BootAnimation();

    sp<SurfaceComposerClient> session() const;

private:
    virtual bool        threadLoop();
    virtual status_t    readyToRun();
    virtual void        onFirstRef();
    virtual void        binderDied(const wp<IBinder>& who);

    struct Texture {
        GLint   w;
        GLint   h;
        GLuint  name;
    };

    struct Animation {
        struct Frame {
            String8 name;
            FileMap* map;
            mutable GLuint tid;
            bool operator < (const Frame& rhs) const {
                return name < rhs.name;
            }
        };
        struct Part {
            int count;
            int pause;
            String8 path;
            SortedVector<Frame> frames;
            bool playUntilComplete;
        };
        int fps;
        int width;
        int height;
        Vector<Part> parts;
    };

    status_t initTexture(Texture* texture, AssetManager& asset, const char* name);
    status_t initTexture(void* buffer, size_t len);
    bool android();
    bool movie();

    void checkExit();

    sp<SurfaceComposerClient>       mSession;
    AssetManager mAssets;
    Texture     mAndroid[2];
    int         mWidth;
    int         mHeight;
    EGLDisplay  mDisplay;
    EGLDisplay  mContext;
    EGLDisplay  mSurface;
    sp<SurfaceControl> mFlingerSurfaceControl;
    sp<Surface> mFlingerSurface;
    bool        mAndroidAnimation;
    ZipFileRO   mZip;
};

// ---------------------------------------------------------------------------

#ifndef ALOGI_IF /* Names changed in Android 4.1 */
#define ALOGI_IF LOGI_IF
#endif
#ifndef ALOGE_IF /* Names changed in Android 4.1 */
#define ALOGE_IF LOGE_IF
#endif
#ifndef ALOGD /* Names changed in Android 4.1 */
#define ALOGD LOGD
#endif

}; // namespace android

#endif // ANDROID_BOOTANIMATION_H
