OBJECTS_fpp_co_USBDMX_so += channeloutput/USBDMX.o
LIBS_fpp_co_USBDMX_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-USBDMX.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_USBDMX_so)

libfpp-co-USBDMX.$(SHLIB_EXT): $(OBJECTS_fpp_co_USBDMX_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_USBDMX_so) $(LIBS_fpp_co_USBDMX_so) $(LDFLAGS) $(LDFLAGS_fpp_co_USBDMX_so) -o $@

