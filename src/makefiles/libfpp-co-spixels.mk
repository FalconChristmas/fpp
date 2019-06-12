ifeq '$(ARCH)' 'Raspberry Pi'
ifneq ($(wildcard /usr/local/include/wiringPi.h),)

OBJECTS_fpp_co_spixels_so += channeloutput/spixels.o
LIBS_fpp_co_spixels_so=-L../external/spixels/lib/ -lspixels -L. -lfpp

CXXFLAGS_channeloutput/spixels.o=-I../external/spixels/include/

TARGETS += libfpp-co-spixels.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_spixels_so)

libfpp-co-spixels.so: libfpp.so $(OBJECTS_fpp_co_spixels_so) ../external/spixels/lib/libspixels.a
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_spixels_so) $(LIBS_fpp_co_spixels_so) $(LDFLAGS) $(LDFLAGS_fpp_co_spixels_so) -o $@

endif
endif
