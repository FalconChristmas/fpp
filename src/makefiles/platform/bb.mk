# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'
ISBEAGLEBONE=1
endif

ifeq '$(ARCH)' 'BeagleBone 64'
ISBEAGLEBONE=1
endif


ifeq ($(ISBEAGLEBONE), 1)

LIBS_GPIO_ADDITIONS=
OBJECTS_GPIO_ADDITIONS+=util/BBBUtils.o
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx

OBJECTS_fpp_so += util/BBBPruUtils.o

BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1
BUILD_FPPRTC=1
BUILD_FPPINIT=1

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so

ARMV := $(shell uname -m 2> /dev/null)
$(shell echo "Building FPP on '$(ARMV)' architecture" 1>&2)

ifeq '$(ARMV)' 'aarch64'
CFLAGS+=-DPLATFORM_BB64
else
# All 32bit Beagles are armv7a processors with neon
CFLAGS+=-DPLATFORM_BBB -march=armv7-a+neon -mfloat-abi=hard -mfpu=neon
endif

LIBS_fpp_so_EXTRA += -L. -lfpp_capeutils
DEPS_fpp_so += libfpp_capeutils.$(SHLIB_EXT)
endif
