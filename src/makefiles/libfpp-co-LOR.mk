OBJECTS_fpp_co_LOR_so += channeloutput/LOR.o
LIBS_fpp_co_LOR_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-LOR.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_LOR_so)

libfpp-co-LOR.$(SHLIB_EXT): $(OBJECTS_fpp_co_LOR_so) $(DEPS_fpp_co_LOR_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_LOR_so) $(LIBS_fpp_co_LOR_so) $(LDFLAGS) $(LDFLAGS_fpp_co_LOR_so) -o $@

