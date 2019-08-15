
OBJECTS_fpp_co_UDPOutput_so += channeloutput/UDPOutput.o  channeloutput/DDP.o channeloutput/E131.o channeloutput/ArtNet.o 
LIBS_fpp_co_UDPOutput_so += -L. -lfpp

TARGETS += libfpp-co-UDPOutput.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_UDPOutput_so)

libfpp-co-UDPOutput.so: $(OBJECTS_fpp_co_UDPOutput_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_UDPOutput_so) $(LIBS_fpp_co_UDPOutput_so) $(LDFLAGS) $(LDFLAGS_fpp_co_UDPOutput_so) -o $@

