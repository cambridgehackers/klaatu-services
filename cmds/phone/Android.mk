LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	klaatuphone_main.cpp PhoneStateMachine.cpp

LOCAL_MODULE:= klaatu_phoneservice
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libklaatu_phone libmedia

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_phoneservice

RILDEBUGSTRINGS := $(LOCAL_PATH)/rildebugstr.generated.h
INCLDIR=hardware/ril/libril

$(RILDEBUGSTRINGS): $(INCLDIR)/ril_commands.h $(INCLDIR)/ril_unsol_commands.h $(LOCAL_PATH)/Android.mk
	cat $(INCLDIR)/ril_commands.h \
	 | egrep "^ *{RIL_" \
	 | sed -re 's/\{RIL_([^,]+),[^,]+,([^}]+).+/case RIL_\1: return "\1";/' >$(RILDEBUGSTRINGS)
	cat $(INCLDIR)/ril_unsol_commands.h \
	 | egrep "^ *{RIL_" \
	 | sed -re 's/\{RIL_([^,]+),([^}]+).+/case RIL_\1: return "\1";/' >>$(RILDEBUGSTRINGS)

$(intermediates)/PhoneStateMachine.o: $(RILDEBUGSTRINGS)
