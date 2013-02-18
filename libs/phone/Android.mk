LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	IPhoneService.cpp \
	IPhoneClient.cpp \
	PhoneClient.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libbinder

LOCAL_MODULE:= libklaatu_phone
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))
ifeq ($(PLATFORM_VERSION),2.3.7)
LOCAL_PRELINK_MODULE := false
endif

include $(BUILD_SHARED_LIBRARY)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/lib/libklaatu_phone.so
