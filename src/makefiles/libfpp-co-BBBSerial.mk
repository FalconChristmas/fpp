# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

OBJECTS_fpp_co_BBBSerial_so += channeloutput/BBBSerial.o
LIBS_fpp_co_BBBSerial_so += -L. -lfpp

TARGETS += libfpp-co-BBBSerial.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBBSerial_so)

libfpp-co-BBBSerial.so: $(OBJECTS_fpp_co_BBBSerial_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBBSerial_so) $(LIBS_fpp_co_BBBSerial_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBBSerial_so) -o $@

endif
