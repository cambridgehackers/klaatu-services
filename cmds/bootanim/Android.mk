LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	bootanimation_main.cpp \
	BootAnimation.cpp

LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libui \
	libskia \
	libEGL \
	libGLESv1_CM \
	libgui

SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))
ifneq ($(PLATFORM_VERSION),4.0.0)
ifneq ($(PLATFORM_VERSION),4.0.4)
LOCAL_SHARED_LIBRARIES += libandroidfw
endif
endif

LOCAL_C_INCLUDES := \
	$(call include-path-for, corecg graphics)

LOCAL_MODULE:= klaatu_bootanimation
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/klaatu_bootanimation

#
# now compile asset files into .apk for use by test program
#

# PRIVATE_ASSET_DIR, PRIVATE_RESOURCE_DIR variables
# are also defined/initialized in build/core/base_rules.mk which overrides the 
# values assigned here
# 
MY_PRIVATE_ASSET_DIR := $(LOCAL_PATH)/assets
MY_PRIVATE_RESOURCE_DIR := $(LOCAL_PATH)/res
MY_PRIVATE_ANDROID_MANIFEST := $(LOCAL_PATH)/AndroidManifest.xml

all_assets := $(addprefix $(MY_PRIVATE_ASSET_DIR)/,$(patsubst assets/%,%,$(call find-subdir-assets,$(MY_PRIVATE_ASSET_DIR))))
all_resources := $(addprefix $(MY_PRIVATE_RESOURCE_DIR)/, $(patsubst res/%,%, $(call find-subdir-assets,$(MY_PRIVATE_RESOURCE_DIR))))
THISAPK := $(TARGET_OUT)/framework/$(LOCAL_MODULE).apk


# add-assests-to-package macro uses PRIVATE_ASSET_DIR, PRIVATE_RESOURCE_DIR variables
# which are also defined/initialized in build/core/base_rules.mk which overrides the 
# values assigned in this make file.. 
# 
#$(create-empty-package)
#$(add-assets-to-package)
$(THISAPK): $(all_assets) $(all_resources) $(PRIVATE_ANDROID_MANIFEST) $(AAPT) $(LOCAL_PATH)/Android.mk
	echo "target APKPackage: $(PRIVATE_MODULE) ($@)"
	$(create-empty-package)
	$(hide) $(AAPT) package -u $(PRIVATE_AAPT_FLAGS) \
		$(addprefix -c , $(PRIVATE_PRODUCT_AAPT_CONFIG)) \
		$(addprefix --preferred-configurations , $(PRIVATE_PRODUCT_AAPT_PREF_CONFIG)) \
		$(addprefix -M , $(MY_PRIVATE_ANDROID_MANIFEST)) \
		$(addprefix -S , $(MY_PRIVATE_RESOURCE_DIR)) \
		$(addprefix -A , $(MY_PRIVATE_ASSET_DIR)) \
		$(addprefix -I , $(PRIVATE_AAPT_INCLUDES)) \
		$(addprefix --min-sdk-version , $(PRIVATE_DEFAULT_APP_TARGET_SDK)) \
		$(addprefix --target-sdk-version , $(PRIVATE_DEFAULT_APP_TARGET_SDK)) \
		$(addprefix --product , $(TARGET_AAPT_CHARACTERISTICS)) \
		$(if $(filter --version-code,$(PRIVATE_AAPT_FLAGS)),,$(addprefix --version-code , $(PLATFORM_SDK_VERSION))) \
		$(if $(filter --version-name,$(PRIVATE_AAPT_FLAGS)),,$(addprefix --version-name , $(PLATFORM_VERSION)-$(BUILD_NUMBER))) \
		$(addprefix --rename-manifest-package , $(PRIVATE_MANIFEST_PACKAGE_NAME)) \
		$(addprefix --rename-instrumentation-target-package , $(PRIVATE_MANIFEST_INSTRUMENTATION_FOR)) \
    	-F $@

$(LOCAL_BUILT_MODULE): $(THISAPK)
