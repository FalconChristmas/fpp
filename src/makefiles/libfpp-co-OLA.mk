OLAFLAGS := $(shell pkg-config --cflags --libs libola 2> /dev/null)
ifneq '$(OLAFLAGS)' ''

OBJECTS_fpp_co_OLA_so += channeloutput/OLAOutput.o
LIBS_fpp_co_OLA_so=-L. -lfpp

TARGETS += libfpp-co-OLA.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_OLA_so)

libfpp-co-OLA.so: $(OBJECTS_fpp_co_OLA_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) $(OBJECTS_fpp_co_OLA_so) $(OLAFLAGS) $(LIBS_fpp_co_OLA_so) $(LDFLAGS) $(LDFLAGS_fpp_co_OLA_so) -o $@

endif
