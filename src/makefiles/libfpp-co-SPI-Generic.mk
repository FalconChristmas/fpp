ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_co_Generic_SPI_so += channeloutput/generic_spi.o
LIBS_fpp_co_Generic_SPI_so=-L. -lfpp

TARGETS += libfpp-co-Generic-SPI.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_Generic_SPI_so)

libfpp-co-Generic-SPI.so: $(OBJECTS_fpp_co_Generic_SPI_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_Generic_SPI_so) $(LIBS_fpp_co_Generic_SPI_so) $(LDFLAGS) $(LDFLAGS_fpp_co_Generic_SPI_so) -o $@

endif
