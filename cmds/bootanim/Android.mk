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

PRIVATE_ASSET_DIR := $(LOCAL_PATH)/assets
PRIVATE_RESOURCE_DIR := $(LOCAL_PATH)/res
PRIVATE_ANDROID_MANIFEST := $(LOCAL_PATH)/AndroidManifest.xml

all_assets := $(addprefix $(PRIVATE_ASSET_DIR)/,$(patsubst assets/%,%,$(call find-subdir-assets,$(PRIVATE_ASSET_DIR))))
all_resources := $(addprefix $(PRIVATE_RESOURCE_DIR)/, $(patsubst res/%,%, $(call find-subdir-assets,$(PRIVATE_RESOURCE_DIR))))
THISAPK := $(TARGET_OUT)/framework/$(LOCAL_MODULE).apk

$(THISAPK): $(all_assets) $(all_resources) $(PRIVATE_ANDROID_MANIFEST) $(AAPT) $(LOCAL_PATH)/Android.mk
	echo "target APKPackage: $(PRIVATE_MODULE) ($@)"
	$(create-empty-package)
	$(add-assets-to-package)

$(LOCAL_BUILT_MODULE): $(THISAPK)
