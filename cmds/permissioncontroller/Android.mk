LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= permissioncontroller_main.cpp 
LOCAL_MODULE:= klaatu_permissioncontroller
LOCAL_MODULE_TAGS:=optional

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils 

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_permissioncontroller
include $(BUILD_EXECUTABLE)
