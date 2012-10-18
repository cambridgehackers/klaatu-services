LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	bootanimation_main.cpp \
	BootAnimation.cpp

LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libandroidfw \
	libutils \
	libbinder \
    libui \
	libskia \
    libEGL \
    libGLESv1_CM \
    libgui

LOCAL_C_INCLUDES := \
	$(call include-path-for, corecg graphics)

LOCAL_MODULE:= klaatu_bootanimation
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

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
