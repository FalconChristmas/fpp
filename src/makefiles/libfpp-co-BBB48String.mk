# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

OBJECTS_fpp_co_BBB48String_so += channeloutput/BBB48String.o
LIBS_fpp_co_BBB48String_so += -Lfpp

TARGETS += libfpp-co-BBB48String.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBB48String_so)

libfpp-co-BBB48String.so: libfpp.so $(OBJECTS_fpp_co_BBB48String_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBB48String_so) $(LIBS_fpp_co_BBB48String_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBB48String_so) -o $@


endif
