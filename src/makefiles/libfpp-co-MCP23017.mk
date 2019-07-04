OBJECTS_fpp_co_MCP23017_so += channeloutput/MCP23017.o
LIBS_fpp_co_MCP23017_so=-L. -lfpp

TARGETS += libfpp-co-MCP23017.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_MCP23017_so)

libfpp-co-MCP23017.so: libfpp.so $(OBJECTS_fpp_co_MCP23017_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_MCP23017_so) $(LIBS_fpp_co_MCP23017_so) $(LDFLAGS) $(LDFLAGS_fpp_co_MCP23017_so) -o $@

