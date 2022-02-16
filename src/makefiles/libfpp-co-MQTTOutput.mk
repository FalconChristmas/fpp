OBJECTS_fpp_co_MQTTOutput_so += channeloutput/MQTTOutput.o
LIBS_fpp_co_MQTTOutput_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-MQTTOutput.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_MQTTOutput_so)

libfpp-co-MQTTOutput.$(SHLIB_EXT): $(OBJECTS_fpp_co_MQTTOutput_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_MQTTOutput_so) $(LIBS_fpp_co_MQTTOutput_so) $(LDFLAGS) $(LDFLAGS_fpp_co_MQTTOutput_so) -o $@

