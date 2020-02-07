ifeq '$(ARCH)' 'Raspberry Pi'
CFLAGS += \
	-DPLATFORM_PI

SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels

OBJECTS_GPIO_ADDITIONS+=util/PiGPIOUtils.o util/PiFaceUtils.o util/MCP23x17Utils.o util/bcm2835.o
CFLAGS_util/bcm2835.o+=-fpermissive

OBJECTS_fppoled += util/SPIUtils.o



endif
