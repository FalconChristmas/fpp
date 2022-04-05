OBJECTS_fpp_co_LOREnhanced_so += channeloutput/LOREnhanced.o
LIBS_fpp_co_LOREnhanced_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-LOREnhanced.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_LOREnhanced_so)

libfpp-co-LOREnhanced.$(SHLIB_EXT): $(OBJECTS_fpp_co_LOREnhanced_so) $(DEPS_fpp_co_LOREnhanced_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_LOREnhanced_so) $(LIBS_fpp_co_LOREnhanced_so) $(LDFLAGS) $(LDFLAGS_fpp_co_LOREnhanced_so) -o $@

