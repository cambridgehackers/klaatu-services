LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= inputserver.cpp
LOCAL_MODULE:= klaatu_inputserver

LOCAL_MODULE_TAGS:=optional
LOCAL_C_INCLUDES := frameworks/base/services/input \
	external/skia/include/core
LOCAL_SHARED_LIBRARIES := libcutils libutils
ifeq ($(PLATFORM_VERSION),2.3.7)
LOCAL_SHARED_LIBRARIES += libui
else
ifeq ($(SHORT_PLATFORM_VERSION),44)
LOCAL_SHARED_LIBRARIES += libinputservice
else
LOCAL_SHARED_LIBRARIES += libinput
endif
endif
SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_inputserver
include $(BUILD_EXECUTABLE)
