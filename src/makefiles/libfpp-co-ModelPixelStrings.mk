OJBECTS_fpp_co_ModelPixelStrings_so += channeloutput/ModelPixelStrings.o
LIBS_fpp_co_ModelPixelStrings_so=-lX11  -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-ModelPixelStrings.$(SHLIB_EXT)
OBJECTS_ALL+=$(OJBECTS_fpp_co_ModelPixelStrings_so)

libfpp-co-ModelPixelStrings.$(SHLIB_EXT): $(OJBECTS_fpp_co_ModelPixelStrings_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(OJBECTS_fpp_co_ModelPixelStrings_so) $(LIBS_fpp_co_ModelPixelStrings_so) $(LDFLAGS) $(LDFLAGS_fpp_co_ModelPixelStrings_so) -o $@
