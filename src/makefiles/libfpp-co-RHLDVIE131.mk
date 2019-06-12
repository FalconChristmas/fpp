
OBJECTS_fpp_co_RHLDVIE131_so += channeloutput/RHL_DVI_E131.o
LIBS_fpp_co_RHLDVIE131_so=-L. -lfpp


TARGETS += libfpp-co-RHLDVIE131.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_RHLDVIE131_so)

libfpp-co-RHLDVIE131.so: libfpp.so $(OBJECTS_fpp_co_RHLDVIE131_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_RHLDVIE131_so) $(LIBS_fpp_co_RHLDVIE131_so) $(LDFLAGS) $(LDFLAGS_fpp_co_RHLDVIE131_so) -o $@

