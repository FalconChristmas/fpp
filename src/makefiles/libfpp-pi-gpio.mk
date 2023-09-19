ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_pi_gpio_so += util/PiGPIOUtils.o util/PiFaceUtils.o util/MCP23x17Utils.o util/bcm2835.o
LIBS_fpp_pi_gpio_so += -Wl,-rpath=$(SRCDIR):. -L. -ljsoncpp

TARGETS += libfpp-pi-gpio.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_pi_gpio_so)

libfpp-pi-gpio.$(SHLIB_EXT): $(OBJECTS_fpp_pi_gpio_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_pi_gpio_so) $(LIBS_fpp_pi_gpio_so) $(LDFLAGS) $(LDFLAGS_fpp_pi_gpio_so) -o $@


endif

