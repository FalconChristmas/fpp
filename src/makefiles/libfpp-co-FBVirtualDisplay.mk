
OBJECTS_fpp_co_FBVirtualDisplay_so += channeloutput/FBVirtualDisplay.o
LIBS_fpp_co_FBVirtualDisplay_so += -L. -lfpp

TARGETS += libfpp-co-FBVirtualDisplay.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_FBVirtualDisplay_so)

libfpp-co-FBVirtualDisplay.so: libfpp.so $(OBJECTS_fpp_co_FBVirtualDisplay_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_FBVirtualDisplay_so) $(LIBS_fpp_co_FBVirtualDisplay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_FBVirtualDisplay_so) -o $@

