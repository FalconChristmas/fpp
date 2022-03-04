OBJECTS_fpp_co_VirtualDisplay_so += channeloutput/VirtualDisplay.o
LIBS_fpp_co_VirtualDisplay_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-VirtualDisplay.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_VirtualDisplay_so)

libfpp-co-VirtualDisplay.$(SHLIB_EXT): $(OBJECTS_fpp_co_VirtualDisplay_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_VirtualDisplay_so) $(LIBS_fpp_co_VirtualDisplay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_VirtualDisplay_so) -o $@

