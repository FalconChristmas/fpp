# BeagleBone 
ifeq ($(ISBEAGLEBONE), 1)

OBJECTS_fpp_co_BBBSerial_so += channeloutput/BBBSerial.o
LIBS_fpp_co_BBBSerial_so += -L. -lfpp -ljsoncpp -lfpp_capeutils

TARGETS += libfpp-co-BBBSerial.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBBSerial_so)

libfpp-co-BBBSerial.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBBSerial_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBBSerial_so) $(LIBS_fpp_co_BBBSerial_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBBSerial_so) -o $@

endif
