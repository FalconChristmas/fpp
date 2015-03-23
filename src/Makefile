CC := g++

ifneq ($(wildcard /usr/bin/ccache),)
	CCACHE = ccache
endif

TARGETS = fpp fppmm fppd

INSTALL_PROGRAM = install -m 755 -p

OBJECTS_fpp = \
	fpp.o \
	fppversion.o \
	$(NULL)
LIBS_fpp = \
	$(NULL)

OBJECTS_fppmm = \
	common.o \
	log.o \
	fppmm.o \
	fppversion.o \
	$(NULL)
LIBS_fppmm = \
	$(NULL)

OBJECTS_fppd = \
	channeloutput/ChannelOutputBase.o \
	channeloutput/channeloutput.o \
	channeloutput/channeloutputthread.o \
	channeloutput/DebugOutput.o \
	channeloutput/E131.o \
	channeloutput/FBMatrix.o \
	channeloutput/FPD.o \
	channeloutput/GPIO.o \
	channeloutput/GPIO595.o \
	channeloutput/LOR.o \
	channeloutput/PanelMatrix.o \
	channeloutput/serialutil.o \
	channeloutput/SPInRF24L01.o \
	channeloutput/SPIws2801.o \
	channeloutput/Triks-C.o \
	channeloutput/USBDMX.o \
	channeloutput/USBPixelnet.o \
	channeloutput/USBRenard.o \
	command.o \
	common.o \
	controlrecv.o \
	controlsend.o \
	e131bridge.o \
	effects.o \
	events.o \
	falcon.o \
	fppd.o \
	fppversion.o \
	gpio.o \
	log.o \
	mediadetails.o \
	mediaoutput/mediaoutput.o \
	mediaoutput/mpg123.o \
	mediaoutput/ogg123.o \
	mediaoutput/omxplayer.o \
	memorymap.o \
	Playlist.o \
	Plugins.o \
	Scheduler.o \
	ScheduleEntry.o \
	Sequence.o \
	settings.o \
	$(NULL)
LIBS_fppd = \
	-lpthread \
	-ljsoncpp \
	-lm \
	-lutil \
	-ltag_c \
	$(NULL)

SUBDIRS = 

ARCH := $(shell cat /etc/fpp/platform 2> /dev/null)
ifeq '$(ARCH)' ''
	ARCH = "UNKNOWN"
endif
$(shell echo "Building FPP on '$(ARCH)' platform" 1>&2)

# Common CFLAGS
CFLAGS += \
	-I . \
	-I channeloutput \
	-I mediaoutput \
	$(NULL)

LDFLAGS_fppd += \
	$(NULL)

DEPS_fppd += \
	$(NULL)

# Do some things a little different when using gcc vs g++
#ifeq ($(CC), gcc)
ifneq (,$(findstring gcc,$(CC)))
# Warn us if we forget to #include something, but this doesn't work on g++
CFLAGS += -Wimplicit
else
# we get lots of deprecated warnings currently with g++ since we're
# normally compiling with gcc
CFLAGS += -Wno-write-strings
endif

ifeq '$(ARCH)' 'Raspberry Pi'
# do something RaspPi-y
CFLAGS += \
	-DPLATFORM_PI \
	-DUSEWIRINGPI \
	-DUSENRF \
	-I../external/RF24/ \
	-I../external/rpi-rgb-led-matrix/include/ \
	-I../external/rpi_ws281x/ \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	-lwiringPiDev \
	-lrf24-bcm \
	-lrgbmatrix \
	-lws2811 \
	$(NULL)
LDFLAGS_fppd += \
	-L../external/RF24/ \
	-L../external/rpi-rgb-led-matrix/lib/ \
	-L../external/rpi_ws281x/ \
	$(NULL)
OBJECTS_fppd += \
	channeloutput/RGBMatrix.o \
	channeloutput/rpi_ws281x.o \
	$(NULL)
DEPS_fppd += \
	../external/RF24/librf24-bcm.so \
	../external/rpi-rgb-led-matrix/lib/librgbmatrix.a \
	../external/rpi_ws281x/libws2811.a \
	$(NULL)
else ifeq '$(ARCH)' 'BeagleBone Black'
# BeagleBone Black
CFLAGS += \
	-DPLATFORM_BBB \
	-I../external/LEDscape/src/ledscape/ \
	$(NULL)
LIBS_fppd += \
	-lledscape \
	-lprussdrv \
	$(NULL)
LDFLAGS_fppd += \
	-L../external/LEDscape/lib/ \
	-L../external/LEDscape/am335x/app_loader/lib/ \
	$(NULL)
OBJECTS_fppd += \
	channeloutput/LEDscapeMatrix.o \
	$(NULL)
DEPS_fppd += \
	../external/LEDscape/lib/libledscape.a \
	$(NULL)
SUBDIRS += pru
else
# do something Linux-y
CFLAGS += \
	-g \
	-DNOROOT \
	$(NULL)
endif

PWD := $(shell pwd)

.PHONY: all
all: $(TARGETS)
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

force:

release: $(TARGETS)
	strip fpp
	-$(INSTALL_PROGRAM) fpp ../bin/fpp
	strip fppmm
	-$(INSTALL_PROGRAM) fppmm ../bin/fppmm
	strip fppd
	-$(INSTALL_PROGRAM) fppd ../bin/fppd
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir release || exit 1; \
	done

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppmm: $(OBJECTS_fppmm)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppd: $(DEPS_fppd) $(OBJECTS_fppd)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppversion.c: fppversion.sh force
	sh fppversion.sh $(PWD)

%.o: %.cpp %.h Makefile
	$(CCACHE) $(CC) $(CFLAGS) -c $< -o $@

%.o: %.c %.h Makefile
	$(CCACHE) $(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f fppversion.c $(OBJECTS_fpp) $(OBJECTS_fppmm) $(OBJECTS_fppd) fpp fppmm fppd
	@if [ -e ../external/RF24/.git ]; then make -C ../external/RF24 clean; fi
	@if [ -e ../external/rpi-rgb-led-matrix/.git ]; then make -C ../external/rpi-rgb-led-matrix clean; fi
	@if [ -e ../external/rpi_ws281x/libws2811.a ]; then rm ../external/rpi_ws281x/*.o ../external/rpi_ws281x/*.a 2> /dev/null; fi
	@if [ -e ../external/LEDscape/.git ]; then make -C ../external/LEDscape clean; fi
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || exit 1; \
	done

#############################################################################
# RF24 library on the Pi
../external/RF24/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/RF24/librf24-bcm.so: ../external/RF24/.git
	@echo "Building RF24 library"
	@make -C ../external/RF24/
	@ln -s librf24-bcm.so.1.0 ../external/RF24/librf24-bcm.so.1
	@ln -s librf24-bcm.so.1 ../external/RF24/librf24-bcm.so

#############################################################################
# RGBMatrix library on the Pi
../external/rpi-rgb-led-matrix/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi-rgb-led-matrix/lib/librgbmatrix.a: ../external/rpi-rgb-led-matrix/.git
	@echo "Building rpi-rgb-led-matrix library"
	@make -C ../external/rpi-rgb-led-matrix/ lib/librgbmatrix.a

#############################################################################
# libws2811 on the Pi
../external/rpi_ws281x/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi_ws281x/libws2811.a: ../external/rpi_ws281x/.git
	@echo "Building libws2811"
	@cd ../external/rpi_ws281x/ && \
		gcc -c -o dma.o dma.c && \
		gcc -c -o pwm.o pwm.c && \
		gcc -c -o ws2811.o ws2811.c && \
		ar rcs libws2811.a dma.o pwm.o ws2811.o

#############################################################################
# LEDscape library and PRU code on the BBB
../external/LEDscape/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/LEDscape/lib/libledscape.a: ../external/LEDscape/.git
	@echo "Building LEDscape library and PRU code"
	@cd ../external/LEDscape/ && git checkout master && git pull
	@make -C ../external/LEDscape
	@cp ../external/LEDscape/lib/matrix.bin ../lib/LEDscapeMatrix.bin
	@cp ../external/LEDscape/lib/ws281x.bin ../lib/LEDscapeWS281x.bin

