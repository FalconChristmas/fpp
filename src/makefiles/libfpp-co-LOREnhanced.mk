OBJECTS_fpp_co_LOREnhanced_so += channeloutput/LOREnhanced.o
LIBS_fpp_co_LOREnhanced_so += -L. -lfpp

TARGETS += libfpp-co-LOREnhanced.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_LOREnhanced_so)

libfpp-co-LOREnhanced.so: $(OBJECTS_fpp_co_LOREnhanced_so) $(DEPS_fpp_co_LOREnhanced_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_LOREnhanced_so) $(LIBS_fpp_co_LOREnhanced_so) $(LDFLAGS) $(LDFLAGS_fpp_co_LOREnhanced_so) -o $@

