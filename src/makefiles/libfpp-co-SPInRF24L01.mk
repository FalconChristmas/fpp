OBJECTS_fpp_co_SPI-nRF24L01_so += channeloutput/SPInRF24L01.o
LIBS_fpp_co_SPI-nRF24L01_so += -L. -lfpp

TARGETS += libfpp-co-SPI-nRF24L01.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_SPI-nRF24L01_so)

ifeq '$(ARCH)' 'Raspberry Pi'
DEPS_fpp_co_SPI-nRF24L01_so += ../external/RF24/librf24-bcm.so
CFLAGS_channeloutput/SPInRF24L01.o+=-DUSENRF -I../external/RF24/
LIBS_fpp_co_SPI-nRF24L01_so += -L../external/RF24/ -lrf24-bcm -Wl,-rpath=.:../external/RF24/
endif

libfpp-co-SPI-nRF24L01.so: $(OBJECTS_fpp_co_SPI-nRF24L01_so) $(DEPS_fpp_co_SPI-nRF24L01_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_SPI-nRF24L01_so) $(LIBS_fpp_co_SPI-nRF24L01_so) $(LDFLAGS) $(LDFLAGS_fpp_co_SPI-nRF24L01_so) -o $@

