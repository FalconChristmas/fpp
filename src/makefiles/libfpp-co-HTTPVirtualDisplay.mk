
OBJECTS_fpp_co_HTTPVirtualDisplay_so += channeloutput/HTTPVirtualDisplay.o
LIBS_fpp_co_HTTPVirtualDisplay_so += -L. -lfpp

TARGETS += libfpp-co-HTTPVirtualDisplay.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_HTTPVirtualDisplay_so)

libfpp-co-HTTPVirtualDisplay.so: libfpp.so $(OBJECTS_fpp_co_HTTPVirtualDisplay_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_HTTPVirtualDisplay_so) $(LIBS_fpp_co_HTTPVirtualDisplay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_HTTPVirtualDisplay_so) -o $@
