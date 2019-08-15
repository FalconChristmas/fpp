OLAFLAGS := $(shell pkg-config --cflags --libs libola 2> /dev/null)
ifneq '$(OLAFLAGS)' ''

SRC_fpp_co_OLA_so += channeloutput/OLAOutput.o
LIBS_fpp_co_OLA_so=-L. -lfpp

TARGETS += libfpp-co-OLA.so

libfpp-co-OLA.so: $(SRC_fpp_co_OLA_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) $(SRC_fpp_co_OLA_so) $(OLAFLAGS) $(LIBS_fpp_co_OLA_so) $(LDFLAGS) $(LDFLAGS_fpp_co_OLA_so) -o $@

endif
