ifeq '$(ARCH)' 'OrangePi'
# do something for Orange Pi

LIBS_GPIO_ADDITIONS=-lwiringPi -lwiringPiDev
OBJECTS_GPIO_ADDITIONS+=util/WiringPiGPIO.o

CXXFLAGS_util/SPIUtils.o+=-DUSEWIRINGPI
CXXFLAGS_util/GPIOUtils.o+=-DUSEWIRINGPI

CFLAGS += \
	-DPLATFORM_ORANGEPI \
	$(NULL)
OBJECTS_fpp_so += \
	channeloutput/Hill320.o \
	$(NULL)
endif
