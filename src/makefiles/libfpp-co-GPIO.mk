OBJECTS_fpp_co_GPIO_so += channeloutput/GPIO.o
LIBS_fpp_co_GPIO_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-GPIO.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_GPIO_so)

libfpp-co-GPIO.$(SHLIB_EXT): $(OBJECTS_fpp_co_GPIO_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GPIO_so) $(LIBS_fpp_co_GPIO_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GPIO_so) -o $@

