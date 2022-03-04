
OBJECTS_fpp_co_UDPOutput_so += channeloutput/UDPOutput.o  channeloutput/DDP.o channeloutput/E131.o channeloutput/ArtNet.o channeloutput/KiNet.o channeloutput/Twinkly.o
LIBS_fpp_co_UDPOutput_so += -L. -lfpp -ljsoncpp -lcurl

TARGETS += libfpp-co-UDPOutput.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_UDPOutput_so)

libfpp-co-UDPOutput.$(SHLIB_EXT): $(OBJECTS_fpp_co_UDPOutput_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_UDPOutput_so) $(LIBS_fpp_co_UDPOutput_so) $(LDFLAGS) $(LDFLAGS_fpp_co_UDPOutput_so) -o $@

