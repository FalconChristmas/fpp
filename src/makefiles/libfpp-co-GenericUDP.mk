
OBJECTS_fpp_co_GenericUDP_so += channeloutput/GenericUDPOutput.o
LIBS_fpp_co_GenericUDP_so=-L. -lfpp -lfpp-co-UDPOutput

TARGETS += libfpp-co-GenericUDP.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_GenericUDP_so)

libfpp-co-GenericUDP.so: $(OBJECTS_fpp_co_GenericUDP_so) libfpp.so libfpp-co-UDPOutput.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GenericUDP_so) $(LIBS_fpp_co_GenericUDP_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GenericUDP_so) -o $@

