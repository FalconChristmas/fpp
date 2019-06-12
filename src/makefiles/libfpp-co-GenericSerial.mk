OBJECTS_fpp_co_GenericSerial_so += channeloutput/GenericSerial.o
LIBS_fpp_co_GenericSerial_so += -L. -lfpp

TARGETS += libfpp-co-GenericSerial.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_GenericSerial_so)

libfpp-co-GenericSerial.so: libfpp.so $(OBJECTS_fpp_co_GenericSerial_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GenericSerial_so) $(LIBS_fpp_co_GenericSerial_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GenericSerial_so) -o $@

