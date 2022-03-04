OBJECTS_fpp_co_SPI-nRF24L01_so += channeloutput/SPInRF24L01.o
LIBS_fpp_co_SPI-nRF24L01_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-SPI-nRF24L01.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_SPI-nRF24L01_so)

ifeq '$(ARCH)' 'Raspberry Pi'
DEPS_fpp_co_SPI-nRF24L01_so += ../external/RF24/librf24-bcm.so
CXXFLAGS_channeloutput/SPInRF24L01.o+=-DUSENRF -I../external/RF24/
LIBS_fpp_co_SPI-nRF24L01_so += -L../external/RF24/ -lrf24-bcm -Wl,-rpath=.:../external/RF24/
LDFLAGS_fpp_so+=-Wl,-rpath=$(PWD):$(PWD)/../external/RF24/
LDFLAGS_fppd+=-Wl,-rpath=$(PWD):$(PWD)/../external/RF24/
endif

libfpp-co-SPI-nRF24L01.$(SHLIB_EXT): $(OBJECTS_fpp_co_SPI-nRF24L01_so) $(DEPS_fpp_co_SPI-nRF24L01_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_SPI-nRF24L01_so) $(LIBS_fpp_co_SPI-nRF24L01_so) $(LDFLAGS) $(LDFLAGS_fpp_co_SPI-nRF24L01_so) -o $@

