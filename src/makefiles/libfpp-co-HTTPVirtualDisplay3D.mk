
OBJECTS_fpp_co_HTTPVirtualDisplay3D_so += channeloutput/HTTPVirtualDisplay3D.o
LIBS_fpp_co_HTTPVirtualDisplay3D_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-HTTPVirtualDisplay3D.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_HTTPVirtualDisplay3D_so)

libfpp-co-HTTPVirtualDisplay3D.$(SHLIB_EXT): $(OBJECTS_fpp_co_HTTPVirtualDisplay3D_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_HTTPVirtualDisplay3D_so) $(LIBS_fpp_co_HTTPVirtualDisplay3D_so) $(LDFLAGS) $(LDFLAGS_fpp_co_HTTPVirtualDisplay3D_so) -o $@

