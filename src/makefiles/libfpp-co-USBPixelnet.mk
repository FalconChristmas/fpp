OBJECTS_fpp_co_USBPixelnet_so += channeloutput/USBPixelnet.o
LIBS_fpp_co_USBPixelnet_so += -L. -lfpp

TARGETS += libfpp-co-USBPixelnet.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_USBPixelnet_so)

libfpp-co-USBPixelnet.so: libfpp.so $(OBJECTS_fpp_co_USBPixelnet_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_USBPixelnet_so) $(LIBS_fpp_co_USBPixelnet_so) $(LDFLAGS) $(LDFLAGS_fpp_co_USBPixelnet_so) -o $@

