OBJECTS_fpp_co_GenericSerial_so += channeloutput/GenericSerial.o
LIBS_fpp_co_GenericSerial_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-GenericSerial.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_GenericSerial_so)

libfpp-co-GenericSerial.$(SHLIB_EXT): $(OBJECTS_fpp_co_GenericSerial_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GenericSerial_so) $(LIBS_fpp_co_GenericSerial_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GenericSerial_so) -o $@

