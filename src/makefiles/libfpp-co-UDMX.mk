OBJECTS_fpp_co_UDMX_so += channeloutput/UDMX.o
LIBS_fpp_co_UDMX_so += -L. -lfpp -ljsoncpp -lusb-1.0

TARGETS += libfpp-co-UDMX.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_UDMX_so)

libfpp-co-UDMX.$(SHLIB_EXT): $(OBJECTS_fpp_co_UDMX_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_UDMX_so) $(LIBS_fpp_co_UDMX_so) $(LDFLAGS) $(LDFLAGS_fpp_co_UDMX_so) -o $@

