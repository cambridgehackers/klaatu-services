LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	klaatuwifi_main.cpp \
	StateMachine.cpp \
	StringUtils.cpp \
	WifiStateMachine.cpp

LOCAL_MODULE:= klaatu_wifiservice
LOCAL_MODULE_TAGS:=optional

ifeq ($(PLATFORM_VERSION),2.3.7)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include external/wpa_supplicant_6/wpa_supplicant
else
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include external/wpa_supplicant_8
endif
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libklaatu_wifi libhardware libhardware_legacy libnetutils
SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))
ifeq ($(word 3,$(SVERSION)),)
LOCAL_CFLAGS += -DLONG_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))0
else
LOCAL_CFLAGS += -DLONG_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))$(word 3,$(SVERSION))
endif

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_wifiservice

