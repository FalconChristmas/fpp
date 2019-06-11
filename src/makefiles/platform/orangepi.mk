ifeq '$(ARCH)' 'OrangePi'
# do something for Orange Pi
CFLAGS += \
	-DPLATFORM_ORANGEPI \
	-DUSEWIRINGPI \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	-lwiringPiDev \
	$(NULL)
OBJECTS_fpp_so += \
	channeloutput/Hill320.o \
	$(NULL)
endif
