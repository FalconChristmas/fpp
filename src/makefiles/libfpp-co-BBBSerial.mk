# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

OBJECTS_fpp_co_BBBSerial_so += channeloutput/BBBSerial.o
LIBS_fpp_co_BBBSerial_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-BBBSerial.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBBSerial_so)

libfpp-co-BBBSerial.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBBSerial_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBBSerial_so) $(LIBS_fpp_co_BBBSerial_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBBSerial_so) -o $@

endif
