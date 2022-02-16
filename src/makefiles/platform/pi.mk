ifeq '$(ARCH)' 'Raspberry Pi'
CFLAGS += \
	-DPLATFORM_PI

SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels

OBJECTS_GPIO_ADDITIONS+=util/PiGPIOUtils.o util/PiFaceUtils.o util/MCP23x17Utils.o util/bcm2835.o
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx

CFLAGS_util/bcm2835.o+=-fpermissive

OBJECTS_fppoled += util/SPIUtils.o

BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so
endif
