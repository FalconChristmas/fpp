#various platforms that need "generic linux flags"
ifeq '$(ARCH)' 'Linux'
ISLINUX=1
CFLAGS += -DPLATFORM_UNKNOWN
else ifeq '$(ARCH)' 'Debian'
ISLINUX=1
CFLAGS += -DPLATFORM_DEBIAN
else ifeq '$(ARCH)' 'Ubuntu'
ISLINUX=1
CFLAGS += -DPLATFORM_UBUNTU
else ifeq '$(ARCH)' 'Mint'
ISLINUX=1
CFLAGS += -DPLATFORM_MINT
else ifeq '$(ARCH)' 'Fedora'
ISLINUX=1
CFLAGS += -DPLATFORM_FEDORA
else ifeq '$(ARCH)' 'Pine64'
ISLINUX=1
CFLAGS += -DPLATFORM_DEBIAN
else ifeq '$(ARCH)' 'CHIP'
ISLINUX=1
CFLAGS += -DPLATFORM_DEBIAN
else ifeq '$(ARCH)' 'Armbian'
ISLINUX=1
CFLAGS += -DPLATFORM_DEBIAN -DPLATFORM_ARMBIAN
else ifeq '$(ARCH)' 'OrangePi'
ISLINUX=1
CFLAGS += -DPLATFORM_DEBIAN -DPLATFORM_ARMBIAN
else ifeq '$(ARCH)' 'UNKNOWN'
ISLINUX=1
CFLAGS += -DPLATFORM_UNKNOWN
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
BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1
BUILD_FPPRTC=1
endif
BUILD_FPPINIT=1

LDFLAGS_fppd += -L.
LDFLAGS_fppd += $(shell log4cpp-config --libs)

OBJECTS_GPIO_ADDITIONS+=
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx
OBJECTS_fppoled += util/SPIUtils.o

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so

endif
