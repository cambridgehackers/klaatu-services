ifneq ($(PLATFORM_VERSION),2.3.3)
ifneq ($(PLATFORM_VERSION),2.3.6)
ifneq ($(PLATFORM_VERSION),2.3.7)
#doesn't work on 2.3
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE:= klaatu_powermanager
LOCAL_SRC_FILES:= powermanager_main.cpp
LOCAL_MODULE_TAGS:= optional
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libpowermanager

SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_powermanager
include $(BUILD_EXECUTABLE)
endif
endif
endif
