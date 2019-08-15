OBJECTS_fpp_co_USBRelay_so += channeloutput/USBRelay.o
LIBS_fpp_co_USBRelay_so += -L. -lfpp

TARGETS += libfpp-co-USBRelay.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_USBRelay_so)

libfpp-co-USBRelay.so: $(OBJECTS_fpp_co_USBRelay_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_USBRelay_so) $(LIBS_fpp_co_USBRelay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_USBRelay_so) -o $@

