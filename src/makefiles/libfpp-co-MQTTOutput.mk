OBJECTS_fpp_co_MQTTOutput_so += channeloutput/MQTTOutput.o
LIBS_fpp_co_MQTTOutput_so += -L. -lfpp

TARGETS += libfpp-co-MQTTOutput.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_MQTTOutput_so)

libfpp-co-MQTTOutput.so: $(OBJECTS_fpp_co_MQTTOutput_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_MQTTOutput_so) $(LIBS_fpp_co_MQTTOutput_so) $(LDFLAGS) $(LDFLAGS_fpp_co_MQTTOutput_so) -o $@

