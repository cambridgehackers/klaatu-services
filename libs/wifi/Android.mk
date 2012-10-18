LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	IWifiService.cpp \
	IWifiClient.cpp \
	WifiClient.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libbinder

LOCAL_MODULE:= libklaatu_wifi
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include

include $(BUILD_SHARED_LIBRARY)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/lib/libklaatu_wifi.so
