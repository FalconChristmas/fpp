OBJECTS_fpp_co_USBDMX_so += channeloutput/USBDMX.o
LIBS_fpp_co_USBDMX_so += -L. -lfpp

TARGETS += libfpp-co-USBDMX.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_USBDMX_so)

libfpp-co-USBDMX.so: $(OBJECTS_fpp_co_USBDMX_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_USBDMX_so) $(LIBS_fpp_co_USBDMX_so) $(LDFLAGS) $(LDFLAGS_fpp_co_USBDMX_so) -o $@

