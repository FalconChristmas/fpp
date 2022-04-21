#various platforms that need "generic linux flags"
ifeq '$(ARCH)' 'Linux'
ISLINUX=1
else ifeq '$(ARCH)' 'Debian'
ISLINUX=1
else ifeq '$(ARCH)' 'Ubuntu'
ISLINUX=1
else ifeq '$(ARCH)' 'Fedora'
ISLINUX=1
else ifeq '$(ARCH)' 'Pine64'
ISLINUX=1
else ifeq '$(ARCH)' 'CHIP'
ISLINUX=1
else ifeq '$(ARCH)' 'Armbian'
ISLINUX=1
else ifeq '$(ARCH)' 'OrangePi'
ISLINUX=1
else ifeq '$(ARCH)' 'UNKNOWN'
ISLINUX=1
endif

ifeq '$(ISLINUX)' '1'
# do something Linux-y

ifeq ($(wildcard /usr/include/Magick++.h),)
CFLAGS += $(shell GraphicsMagick++-config --cppflags)
endif

ifneq ($(wildcard /.dockerenv),)
#don't build the oled/cape stuff for docker
CFLAGS += -DPLATFORM_DOCKER
else
CFLAGS += -DPLATFORM_UNKNOWN
BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1
endif


LDFLAGS_fppd += -L.
LDFLAGS_fppd += $(shell log4cpp-config --libs)

OBJECTS_GPIO_ADDITIONS+=
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx
OBJECTS_fppoled += util/SPIUtils.o
BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so

endif
