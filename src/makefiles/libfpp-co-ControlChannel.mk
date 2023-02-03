
OBJECTS_fpp_co_ControlChannel_so += channeloutput/ControlChannelOutput.o
LIBS_fpp_co_ControlChannel_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-ControlChannel.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_ControlChannel_so)

libfpp-co-ControlChannel.$(SHLIB_EXT): $(OBJECTS_fpp_co_ControlChannel_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(LDFLAGS) $(LIBS_fpp_co_ControlChannel_so) $(LDFLAGS_fpp_co_ControlChannel_so) $(OBJECTS_fpp_co_ControlChannel_so) -o $@

