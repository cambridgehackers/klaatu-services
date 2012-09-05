LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE:= powermanager
LOCAL_SRC_FILES:= powermanager_main.cpp
LOCAL_MODULE_TAGS:= optional
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libpowermanager

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/powermanager
include $(BUILD_EXECUTABLE)
