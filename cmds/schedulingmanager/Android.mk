LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= schedulingmanager_main.cpp
LOCAL_MODULE:= klaatu_schedulingmanager
LOCAL_MODULE_TAGS:=optional
LOCAL_C_INCLUDES := frameworks/av/services/audioflinger

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils
LOCAL_STATIC_LIBRARIES := libscheduling_policy

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/schedulingmanager
include $(BUILD_EXECUTABLE)
