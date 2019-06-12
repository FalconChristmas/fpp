
OBJECTS_fpp_co_Debug_so += channeloutput/DebugOutput.o
LIBS_fpp_co_Debug_so += -L. -lfpp

TARGETS += libfpp-co-Debug.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_Debug_so)

libfpp-co-Debug.so: libfpp.so $(OBJECTS_fpp_co_Debug_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_Debug_so) $(LIBS_fpp_co_Debug_so) $(LDFLAGS) $(LDFLAGS_fpp_co_Debug_so) -o $@

