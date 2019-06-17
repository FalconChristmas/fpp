OBJECTS_fpp_co_LOR_so += channeloutput/LOR.o
LIBS_fpp_co_LOR_so += -L. -lfpp

TARGETS += libfpp-co-LOR.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_LOR_so)

libfpp-co-LOR.so: libfpp.so $(OBJECTS_fpp_co_LOR_so) $(DEPS_fpp_co_LOR_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_LOR_so) $(LIBS_fpp_co_LOR_so) $(LDFLAGS) $(LDFLAGS_fpp_co_LOR_so) -o $@

