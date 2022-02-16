OBJECTS_fpp_co_USBPixelnet_so += channeloutput/USBPixelnet.o
LIBS_fpp_co_USBPixelnet_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-USBPixelnet.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_USBPixelnet_so)

libfpp-co-USBPixelnet.$(SHLIB_EXT): $(OBJECTS_fpp_co_USBPixelnet_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_USBPixelnet_so) $(LIBS_fpp_co_USBPixelnet_so) $(LDFLAGS) $(LDFLAGS_fpp_co_USBPixelnet_so) -o $@

