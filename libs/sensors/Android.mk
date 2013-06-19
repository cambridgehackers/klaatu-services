LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	sensor.cpp accelerationsensor.cpp

LOCAL_CFLAGS:= -I.. -DKLAATU_SENSOR_LIB

LOCAL_C_INCLUDES += external/klaatu-services/include
LOCAL_SHARED_LIBRARIES := \
	libcutils libutils libui libgui libhardware

LOCAL_MODULE:= libklaatu_sensors

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/lib/libklaatu_sensors.so
