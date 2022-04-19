ifeq '$(ARCH)' 'OrangePi'
# do something for Orange Pi

CFLAGS += \
	-DPLATFORM_ORANGEPI \
	$(NULL)
	
	
OBJECTS_GPIO_ADDITIONS+=
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx

OBJECTS_fppoled += util/SPIUtils.o

BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so

endif
