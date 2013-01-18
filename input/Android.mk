LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= inputserver.cpp
LOCAL_MODULE:= klaatu_inputserver

LOCAL_MODULE_TAGS:=optional
LOCAL_C_INCLUDES := frameworks/base/services/input \
	external/skia/include/core
LOCAL_SHARED_LIBRARIES := libinput libcutils libutils

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_inputserver
include $(BUILD_EXECUTABLE)
