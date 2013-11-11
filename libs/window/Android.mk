LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

libklaatu_window_SRC_FILES := \
    libklaatu_window.h \
    libklaatu_window.cpp

# Static library for target
# ========================================================
include $(CLEAR_VARS)
SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))
LOCAL_CFLAGS += -rdynamic
LOCAL_MODULE := libklaatu_window
LOCAL_MODULE_CLASS    := SHARED_LIBRARIES
LOCAL_SRC_FILES := $(libklaatu_window_SRC_FILES)
LOCAL_C_INCLUDES := frameworks/base/services \
        external/skia/include/core \
        bionic \
        external/stlport/stlport \
        external/jpeg \
        external/libpng \
        external/zlib \
        external/freetype/include \
        frameworks/base/include/surfaceflinger \
        frameworks/native/opengl/include \
        $(LOCAL_PATH)/src/sg/fonts \
        $(LOCAL_PATH)/prebuilt/include

LOCAL_SHARED_LIBRARIES := \
    libEGL libGLESv2 libui libgui \
    libutils libstlport libinput \
    libjpeg \
    libmedia libbinder libcutils \
    libz

include $(BUILD_SHARED_LIBRARY)

