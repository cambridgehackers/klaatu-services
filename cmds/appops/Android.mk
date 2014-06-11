LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
SHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(SHORT_PLATFORM_VERSION)

ifeq ($(SHORT_PLATFORM_VERSION),43)
BUILD=true
else ifeq ($(SHORT_PLATFORM_VERSION),44)
BUILD=true
endif

ifneq ($(BUILD),)
LOCAL_SRC_FILES:= appops_main.cpp 
LOCAL_MODULE:= klaatu_appops
LOCAL_MODULE_TAGS:=optional

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils 

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_appops
include $(BUILD_EXECUTABLE)
endif
