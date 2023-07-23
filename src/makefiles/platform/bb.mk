# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

CFLAGS += \
	-DPLATFORM_BBB

LIBS_GPIO_ADDITIONS=
OBJECTS_GPIO_ADDITIONS+=util/BBBUtils.o
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx

OBJECTS_fpp_so += util/BBBPruUtils.o

BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so

# Beagles are ALL armv7a processors with neon
CFLAGS+=-march=armv7-a+neon -mfloat-abi=hard -mfpu=neon

endif
