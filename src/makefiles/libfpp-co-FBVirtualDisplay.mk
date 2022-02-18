ifneq '$(ARCH)' 'OSX'

OBJECTS_fpp_co_FBVirtualDisplay_so += channeloutput/FBVirtualDisplay.o
LIBS_fpp_co_FBVirtualDisplay_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-FBVirtualDisplay.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_FBVirtualDisplay_so)

libfpp-co-FBVirtualDisplay.$(SHLIB_EXT): $(OBJECTS_fpp_co_FBVirtualDisplay_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_FBVirtualDisplay_so) $(LIBS_fpp_co_FBVirtualDisplay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_FBVirtualDisplay_so) -o $@

endif
