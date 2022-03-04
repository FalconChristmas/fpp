OBJECTS_fpp_co_MCP23017_so += channeloutput/MCP23017.o
LIBS_fpp_co_MCP23017_so=-L. -lfpp -ljsoncpp

TARGETS += libfpp-co-MCP23017.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_MCP23017_so)

libfpp-co-MCP23017.$(SHLIB_EXT): $(OBJECTS_fpp_co_MCP23017_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_MCP23017_so) $(LIBS_fpp_co_MCP23017_so) $(LDFLAGS) $(LDFLAGS_fpp_co_MCP23017_so) -o $@

