ifeq '$(ARCH)' 'Raspberry Pi'
CFLAGS += \
	-DPLATFORM_PI

SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels

LIBS_GPIO_ADDITIONS=-lpigpio
OBJECTS_GPIO_ADDITIONS+=util/PiGPIOUtils.o util/PiFaceUtils.o util/MCP23x17Utils.o
CXXFLAGS_util/GPIOUtils.o+=-DUSEPIGPIO

OBJECTS_fppoled += util/SPIUtils.o



endif
