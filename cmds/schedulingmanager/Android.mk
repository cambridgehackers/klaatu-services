LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= schedulingmanager_main.cpp
LOCAL_MODULE:= klaatu_schedulingmanager
LOCAL_MODULE_TAGS:=optional
LOCAL_C_INCLUDES := frameworks/av/services/audioflinger

SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils
LOCAL_STATIC_LIBRARIES := libscheduling_policy

ifneq ($(PLATFORM_VERSION),4.0.0)
ifneq ($(PLATFORM_VERSION),4.0.4)
ifneq ($(PLATFORM_VERSION),2.3.7)
ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_schedulingmanager
include $(BUILD_EXECUTABLE)
endif
endif
endif
