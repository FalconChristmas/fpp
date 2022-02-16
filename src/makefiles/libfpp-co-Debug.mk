
OBJECTS_fpp_co_Debug_so += channeloutput/DebugOutput.o
LIBS_fpp_co_Debug_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-Debug.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_Debug_so)

libfpp-co-Debug.$(SHLIB_EXT): $(OBJECTS_fpp_co_Debug_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_Debug_so) $(LIBS_fpp_co_Debug_so) $(LDFLAGS) $(LDFLAGS_fpp_co_Debug_so) -o $@

