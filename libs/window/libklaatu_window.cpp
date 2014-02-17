#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <binder/IPCThreadState.h>
#include <ui/DisplayInfo.h>
#include <ui/FramebufferNativeWindow.h>
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION <= 40)
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <utils/ResourceTypes.h>
#elif (SHORT_PLATFORM_VERSION == 41)
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <androidfw/ResourceTypes.h>
#else // Android version 4.2 and above
#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <androidfw/ResourceTypes.h>
#endif
extern "C" {
#include "libklaatu_window.h"
}

#pragma message(VAR_NAME_VALUE(SHORT_PLATFORM_VERSION))
using namespace std;
using android::sp;

class klaatu_video {
    public:
	static unsigned int get_native_window(int w, int h);
	static void get_display_size(int* w, int* h);
};

extern "C" {
	unsigned int klaatu_get_window(int w, int h) {
		return klaatu_video::get_native_window(w, h);
	}

	void klaatu_get_display_size(int* w, int* h){
		return klaatu_video::get_display_size(w, h);
	}
};

static EGLDisplay mEglDisplay = EGL_NO_DISPLAY;
static EGLSurface mEglSurface = EGL_NO_SURFACE;
static EGLContext mEglContext = EGL_NO_CONTEXT;
static sp<android::SurfaceComposerClient> mSession;
static sp<android::SurfaceControl> mControl;
static sp<android::Surface> mAndroidSurface;

void klaatu_video::get_display_size(int* w, int* h)
{
	android::DisplayInfo dinfo;
	mSession = new android::SurfaceComposerClient();

	#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION >= 42)
		sp<android::IBinder> dtoken(android::SurfaceComposerClient::getBuiltInDisplay(android::ISurfaceComposer::eDisplayIdMain));
		int status = mSession->getDisplayInfo(dtoken, &dinfo);
	#else
		int status = mSession->getDisplayInfo(0, &dinfo);
	#endif

	if (status)
		{
		*w = *h = 0;
		return;
		}

	*w = dinfo.w;
	*h = dinfo.h;

	return;
}

unsigned int klaatu_video::get_native_window(int w, int h)
{
	android::DisplayInfo dinfo;
	mSession = new android::SurfaceComposerClient();

#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION >= 42)
	sp<android::IBinder> dtoken(android::SurfaceComposerClient::getBuiltInDisplay(android::ISurfaceComposer::eDisplayIdMain));
	int status = mSession->getDisplayInfo(dtoken, &dinfo);
#else
	int status = mSession->getDisplayInfo(0, &dinfo);
#endif

	if (status)
        	return 0;

	if (w <= 0)
		w = dinfo.w;

	if (h <= 0)
		h = dinfo.h;

	mControl = mSession->createSurface(
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION >= 42)
		android::String8("libklaatuwindow"),
#elif defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
		0, android::String8("libklaatuwindow"), 0,
#else
		0,
#endif
		w, h, android::PIXEL_FORMAT_RGB_888, 0);

	android::SurfaceComposerClient::openGlobalTransaction();
	mControl->setLayer(0x40000000);
	android::SurfaceComposerClient::closeGlobalTransaction();
	mAndroidSurface = mControl->getSurface();
	EGLNativeWindowType eglWindow = mAndroidSurface.get();

	return (unsigned int)eglWindow;
}


