ifeq '$(ARCH)' 'Raspberry Pi'
CFLAGS += \
	-DPLATFORM_PI

SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels

OBJECTS_GPIO_ADDITIONS+=
LIBS_GPIO_ADDITIONS+=-lgpiod -lgpiodcxx


CFLAGS_util/bcm2835.o+=-fpermissive

OBJECTS_fppoled += util/SPIUtils.o

BUILD_FPPOLED=1
BUILD_FPPCAPEDETECT=1

LDFLAGS=-lrt -lpthread
SHLIB_EXT=so


LIBS_GPIO_EXE_ADDITIONS+=$(LIBS_GPIO_ADDITIONS) -lfpp-pi-gpio
DEPENDENCIES_GPIO_ADDITIONS+=libfpp-pi-gpio.$(SHLIB_EXT)

OBJECTS_fpp_pi_gpio_so += util/PiGPIOUtils.o util/PiFaceUtils.o util/MCP23x17Utils.o util/bcm2835.o
LIBS_fpp_pi_gpio_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-pi-gpio.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_pi_gpio_so)

libfpp-pi-gpio.$(SHLIB_EXT): $(OBJECTS_fpp_pi_gpio_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_pi_gpio_so) $(LIBS_fpp_pi_gpio_so) $(LDFLAGS) $(LDFLAGS_fpp_pi_gpio_so) -o $@

endif