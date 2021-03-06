ifneq ($(PLATFORM_VERSION),2.3.7)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	audioconfig_main.cpp
LOCAL_MODULE:= klaatu_audioconfig
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libmedia
SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_audioconfig

endif
