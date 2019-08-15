OBJECTS_fpp_co_Hill320_so += channeloutput/Hill320.o
LIBS_fpp_co_Hill320_so=-L. -lfpp


TARGETS += libfpp-co-Hill320.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_Hill320_so)

libfpp-co-Hill320.so: $(OBJECTS_fpp_co_Hill320_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_Hill320_so) $(LIBS_fpp_co_Hill320_so) $(LDFLAGS) $(LDFLAGS_fpp_co_Hill320_so) -o $@

