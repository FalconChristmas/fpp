#various platforms that need "generic linux flags"
ifeq '$(ARCH)' 'Linux'
ISLINUX=1
else ifeq '$(ARCH)' 'Debian'
ISLINUX=1
else ifeq '$(ARCH)' 'Ubuntu'
ISLINUX=1
else ifeq '$(ARCH)' 'Pine64'
ISLINUX=1
else ifeq '$(ARCH)' 'CHIP'
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
CFLAGS += -DPLATFORM_DOCKER
else
CFLAGS += -DPLATFORM_UNKNOWN
endif

LDFLAGS_fppd += -L.

LDFLAGS_fppd += $(shell log4cpp-config --libs)

OBJECTS_GPIO_ADDITIONS+=util/TmpFileGPIO.o

endif
