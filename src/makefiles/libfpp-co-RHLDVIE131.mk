
OBJECTS_fpp_co_RHLDVIE131_so += channeloutput/RHL_DVI_E131.o
LIBS_fpp_co_RHLDVIE131_so=-L. -lfpp -ljsoncpp


TARGETS += libfpp-co-RHLDVIE131.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_RHLDVIE131_so)

libfpp-co-RHLDVIE131.$(SHLIB_EXT): $(OBJECTS_fpp_co_RHLDVIE131_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_RHLDVIE131_so) $(LIBS_fpp_co_RHLDVIE131_so) $(LDFLAGS) $(LDFLAGS_fpp_co_RHLDVIE131_so) -o $@

