OBJECTS_fpp_co_PCF8574_so += channeloutput/PCF8574.o
LIBS_fpp_co_PCF8574_so=-L. -lfpp -ljsoncpp

TARGETS += libfpp-co-PCF8574.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_PCF8574_so)

libfpp-co-PCF8574.$(SHLIB_EXT): $(OBJECTS_fpp_co_PCF8574_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_PCF8574_so) $(LIBS_fpp_co_PCF8574_so) $(LDFLAGS) $(LDFLAGS_fpp_co_PCF8574_so) -o $@

