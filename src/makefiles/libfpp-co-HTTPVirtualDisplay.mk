
OBJECTS_fpp_co_HTTPVirtualDisplay_so += channeloutput/HTTPVirtualDisplay.o
LIBS_fpp_co_HTTPVirtualDisplay_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-HTTPVirtualDisplay.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_HTTPVirtualDisplay_so)

libfpp-co-HTTPVirtualDisplay.$(SHLIB_EXT): $(OBJECTS_fpp_co_HTTPVirtualDisplay_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_HTTPVirtualDisplay_so) $(LIBS_fpp_co_HTTPVirtualDisplay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_HTTPVirtualDisplay_so) -o $@
