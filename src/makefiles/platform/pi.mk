ifeq '$(ARCH)' 'Raspberry Pi'
CFLAGS += \
	-DPLATFORM_PI

# PLATFORM_PI is set on both 32- and 64-bit Pi builds; PLATFORM_PI64 is set
# only when the compiler itself targets aarch64. Must be in the makefile
# (not only in fpp-pch.h) because many plugins don't include the PCH but
# inherit CFLAGS from here.
#
# IMPORTANT: detect via `$(CXX) -dumpmachine`, NOT `uname -m`. On a Pi4/Pi5
# running 32-bit Raspberry Pi OS the default config.txt boots a 64-bit kernel
# -- `uname -m` then returns "aarch64" even though the compiler, userspace,
# and produced binary are all armhf. -dumpmachine reflects the compiler's
# actual target triple, which is what we care about.
ifneq ($(findstring aarch64,$(shell $(CXX) -dumpmachine 2>/dev/null)),)
CFLAGS += -DPLATFORM_PI64
else
# 32-bit Raspberry Pi OS targets ARMv6 (Pi Zero/W, Pi 1) so one armhf image runs
# on every Pi. RPiOS's gcc only DEFAULTS to that arch -- make it EXPLICIT so a
# distcc/nocc offloaded build (whose helper compiler defaults to ARMv7) still
# produces ARMv6 objects that run on a Pi Zero. -marm is required because ARMv6
# has no Thumb-2 and Thumb-1 + hard-float VFP is unsupported (the helper's gcc
# defaults to -mthumb). Mirrors the RF24 armhf flags in modules.mk, which build
# locally with the native (ARM-mode-default) gcc and so don't need -marm.
CFLAGS += -march=armv6zk -mfpu=vfp -mfloat-abi=hard -marm -mtune=arm1176jzf-s
endif

SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels

OBJECTS_GPIO_ADDITIONS+=
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx


OBJECTS_fppoled += util/SPIUtils.o

BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1
BUILD_FPPRTC=1
BUILD_FPPINIT=1

LDFLAGS+=-lrt -lpthread
SHLIB_EXT=so

LIBS_GPIO_EXE_ADDITIONS+=$(LIBS_GPIO_ADDITIONS) -lfpp-pi-gpio
DEPENDENCIES_GPIO_ADDITIONS+=libfpp-pi-gpio.$(SHLIB_EXT)

LIBS_fpp_so_EXTRA += -L. -lfpp_capeutils
DEPS_fpp_so += libfpp_capeutils.$(SHLIB_EXT)

endif
