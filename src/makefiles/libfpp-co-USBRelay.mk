OBJECTS_fpp_co_USBRelay_so += channeloutput/USBRelay.o
LIBS_fpp_co_USBRelay_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-USBRelay.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_USBRelay_so)

libfpp-co-USBRelay.$(SHLIB_EXT): $(OBJECTS_fpp_co_USBRelay_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_USBRelay_so) $(LIBS_fpp_co_USBRelay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_USBRelay_so) -o $@

