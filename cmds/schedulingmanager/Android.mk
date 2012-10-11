LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= schedulingmanager_main.cpp
LOCAL_MODULE:= schedulingmanager
LOCAL_MODULE_TAGS:=optional

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils 

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/schedulingmanager
include $(BUILD_EXECUTABLE)
