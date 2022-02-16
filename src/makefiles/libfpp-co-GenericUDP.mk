
OBJECTS_fpp_co_GenericUDP_so += channeloutput/GenericUDPOutput.o
LIBS_fpp_co_GenericUDP_so=-L. -lfpp -lfpp-co-UDPOutput -ljsoncpp

TARGETS += libfpp-co-GenericUDP.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_GenericUDP_so)

libfpp-co-GenericUDP.$(SHLIB_EXT): $(OBJECTS_fpp_co_GenericUDP_so) libfpp.$(SHLIB_EXT) libfpp-co-UDPOutput.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GenericUDP_so) $(LIBS_fpp_co_GenericUDP_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GenericUDP_so) -o $@

