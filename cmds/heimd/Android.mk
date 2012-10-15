LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	heimd_main.cpp \
	HeimdService.cpp \
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

LOCAL_MODULE:= heimd
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libheimd libhardware libhardware_legacy libnetutils

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/heimd

