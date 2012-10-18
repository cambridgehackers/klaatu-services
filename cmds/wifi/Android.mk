LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	klaatuwifi_main.cpp \
	WifiService.cpp \
	DaemonConnector.cpp \
	StateMachine.cpp \
	StringUtils.cpp \
	WifiMonitor.cpp \
	WifiCommands.cpp \
	ScannedStations.cpp \
	WifiConfigStore.cpp \
	NetworkInterface.cpp \
	DhcpStateMachine.cpp \
	WifiStateMachine.cpp

LOCAL_MODULE:= klaatu_wifiservice
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libklaatu_wifi libhardware libhardware_legacy libnetutils

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_wifiservice

