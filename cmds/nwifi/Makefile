
BASEDIR=/Users/jca/android55

GPP = $(BASEDIR)/prebuilts/gcc/darwin-x86/arm/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-g++ 

GPP_INCLUDES = -I . -I ../../include \
    -I $(BASEDIR)/out/target/product/maguro/obj/EXECUTABLES/klaatu_wifiservice_intermediates \
    -I $(BASEDIR)/libnativehelper/include/nativehelper \
       @$(BASEDIR)/out/target/product/maguro/obj/EXECUTABLES/klaatu_wifiservice_intermediates/import_includes \
    -isystem $(BASEDIR)/system/core/include \
    -isystem $(BASEDIR)/hardware/libhardware/include \
    -isystem $(BASEDIR)/hardware/libhardware_legacy/include \
    -isystem $(BASEDIR)/hardware/ril/include \
    -isystem $(BASEDIR)/libnativehelper/include \
    -isystem $(BASEDIR)/frameworks/native/include \
    -isystem $(BASEDIR)/frameworks/native/opengl/include \
    -isystem $(BASEDIR)/frameworks/av/include \
    -isystem $(BASEDIR)/frameworks/base/include \
    -isystem $(BASEDIR)/frameworks/base/opengl/include \
    -isystem $(BASEDIR)/external/skia/include \
    -isystem $(BASEDIR)/out/target/product/maguro/obj/include \
    -isystem $(BASEDIR)/hardware/ti/omap4xxx/kernel-headers \
    -isystem $(BASEDIR)/bionic/libc/arch-arm/include \
    -isystem $(BASEDIR)/bionic/libc/include \
    -isystem $(BASEDIR)/bionic/libstdc++/include \
    -isystem $(BASEDIR)/bionic/libc/kernel/common \
    -isystem $(BASEDIR)/bionic/libc/kernel/arch-arm \
    -isystem $(BASEDIR)/bionic/libm/include \
    -isystem $(BASEDIR)/bionic/libm/include/arm \
    -isystem $(BASEDIR)/bionic/libthread_db/include

GPP_FLAGS = \
    -fno-exceptions \
    -Wno-multichar \
    -msoft-float \
    -fpic -fPIE -ffunction-sections -fdata-sections -funwind-tables -fstack-protector \
    -Wa,--noexecstack -Werror=format-security \
    -fno-short-enums \
    -march=armv7-a -mfloat-abi=softfp -mfpu=neon \
    -include $(BASEDIR)/system/core/include/arch/linux-arm/AndroidConfig.h \
    -I $(BASEDIR)/system/core/include/arch/linux-arm/ \
    -Wno-unused-but-set-variable \
    -fno-builtin-sin -fno-strict-volatile-bitfields \
    -Wno-psabi \
    -mthumb-interwork \
    -DANDROID \
    -fmessage-length=0 \
    -W -Wall -Wno-unused -Winit-self -Wpointer-arith -Werror=return-type -Werror=non-virtual-dtor \
    -Werror=address -Werror=sequence-point \
    -DNDEBUG \
    -Wstrict-aliasing=2 \
    -fgcse-after-reload -frerun-cse-after-loop -frename-registers \
    -UDEBUG \
    -fvisibility-inlines-hidden -fmessage-length=0 \
    -Wsign-promo \
    -mthumb \
    -Os \
    -fomit-frame-pointer -fno-strict-aliasing  -fno-rtti

COMPILE=$(GPP) $(GPP_FLAGS) $(GPP_INCLUDES)

OBJS = NetworkInterface.o \
    StateMachine.o StringUtils.o WifiConfigStore.o \
    WifiStateMachine.o klaatuwifi_main.o

all:
	@echo WifiStateMachine.cpp; $(COMPILE) -c WifiStateMachine.cpp
	@echo StateMachine.cpp; $(COMPILE) -c StateMachine.cpp
	@echo NetworkInterface.cpp; $(COMPILE) -c NetworkInterface.cpp
	@echo StringUtils.cpp; $(COMPILE) -c StringUtils.cpp
	@echo klaatuwifi_main.cpp; $(COMPILE) -c klaatuwifi_main.cpp

#@echo link; $(COMPILE) $(OBJS)
