CC := g++

ifneq ($(wildcard /usr/bin/ccache),)
	CCACHE = ccache
endif

TARGETS = fpp fppmm fppd
SUBMODULES =

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
	channeloutput/ArtNet.o \
	channeloutput/ColorOrder.o \
	channeloutput/ColorLight-5a-75.o \
	channeloutput/DebugOutput.o \
	channeloutput/DDP.o \
	channeloutput/E131.o \
	channeloutput/FBMatrix.o \
	channeloutput/FBVirtualDisplay.o \
	channeloutput/FPD.o \
	channeloutput/GenericSerial.o \
	channeloutput/GPIO.o \
	channeloutput/GPIO595.o \
	channeloutput/Linsn-RV9.o \
	channeloutput/LOR.o \
	channeloutput/Matrix.o \
	channeloutput/MCP23017.o \
	channeloutput/PanelMatrix.o \
	channeloutput/PixelString.o \
	channeloutput/RHL_DVI_E131.o \
	channeloutput/serialutil.o \
	channeloutput/SPInRF24L01.o \
	channeloutput/SPIws2801.o \
	channeloutput/Triks-C.o \
	channeloutput/USBDMX.o \
	channeloutput/USBPixelnet.o \
	channeloutput/USBRelay.o \
	channeloutput/USBRenard.o \
	channeloutput/VirtualDisplay.o \
	channeltester/ChannelTester.o \
	channeltester/TestPatternBase.o \
	channeltester/RGBChase.o \
	channeltester/RGBFill.o \
	channeltester/SingleChase.o \
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
	mediaoutput/MediaOutputBase.o \
	mediaoutput/mediaoutput.o \
	mediaoutput/mpg123.o \
	mediaoutput/ogg123.o \
	mediaoutput/omxplayer.o \
	PixelOverlay.o \
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
	-ltag \
	$(NULL)

SUBDIRS = 

BINDIR = ../bin

ARCH := $(shell cat /etc/fpp/platform 2> /dev/null)
ifeq '$(ARCH)' ''
	ARCH = "UNKNOWN"
endif
$(shell echo "Building FPP on '$(ARCH)' platform" 1>&2)

OLAFLAGS := $(shell pkg-config --cflags --libs libola 2> /dev/null)
ifneq '$(OLAFLAGS)' ''
	CFLAGS += -DUSEOLA
	LDFLAGS_fppd += $(OLAFLAGS)
	OBJECTS_fppd += channeloutput/OLAOutput.o
endif

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
CFLAGS += -Wno-write-strings -Wno-narrowing
endif

ifeq '$(ARCH)' 'Raspberry Pi'
# do something RaspPi-y
BINDIR = ../bin.pi
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
	channeloutput/Hill320.o \
	channeloutput/RGBMatrix.o \
	channeloutput/rpi_ws281x.o \
	$(NULL)
DEPS_fppd += \
	../external/RF24/librf24-bcm.so \
	../lib/librgbmatrix.so \
	../external/rpi_ws281x/libws2811.a \
	$(NULL)
SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	$(NULL)
else ifeq '$(ARCH)' 'ODROID'
# do something for ODROID
BINDIR = ../bin.pi
CFLAGS += \
	-DPLATFORM_ODROID \
	-DUSEWIRINGPI \
	-I../external/rpi-rgb-led-matrix/include/ \
	$(NULL)
LIBS_fppd += \
	-lrgbmatrix \
	-lwiringPi \
	-lwiringPiDev \
	$(NULL)
LDFLAGS_fppd += \
	-L../external/rpi-rgb-led-matrix/lib/ \
	$(NULL)
OBJECTS_fppd += \
	channeloutput/RGBMatrix.o \
	$(NULL)
DEPS_fppd += \
	../lib/librgbmatrix.so \
	$(NULL)
SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	$(NULL)
else ifeq '$(ARCH)' 'BeagleBone Black'
# BeagleBone Black
CXXFLAGS += -std=gnu++14
BINDIR = ../bin.bbb
CFLAGS += \
    -DNOROOT \
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
	channeloutput/BBB48String.o \
	channeloutput/BBBSerial.o \
    channeloutput/BBBUtils.o \
	$(NULL)
DEPS_fppd += \
	../external/LEDscape/lib/libledscape.a \
	$(NULL)
SUBMODULES += \
	external/LEDscape \
	$(NULL)
else
# do something Linux-y
BINDIR = ../bin
CFLAGS += \
	-g \
	-DNOROOT \
	$(NULL)
endif

##############################################################################
# Include a local Makefile if one exists to modify any of the above
-include Makefile.local
#
# Examples of some entries that might exist in Makefile.local:
#
# LIBS_fppd += -lX11
# CFLAGS += -DUSE_X11Matrix
# OBJECTS_fppd += channeloutput/X11Matrix.o
##############################################################################

PWD := $(shell pwd)

.PHONY: all
all: gitsubmodules FPP subdirs

subdirs:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

FPP: $(TARGETS)

.PHONY: gitsubmodules
gitsubmodules:
	@! ping -q -c 1 github.com > /dev/null 2>&1 || (cd ..; \
		for submodule in $(SUBMODULES); do \
			git submodule sync $$submodule; \
		done; \
		git submodule foreach git fetch; \
		for submodule in $(SUBMODULES); do \
			git submodule update --init $$submodule; \
		done)

force:

release: $(TARGETS)
	strip fpp
	-$(INSTALL_PROGRAM) fpp $(BINDIR)/fpp
	strip fppmm
	-$(INSTALL_PROGRAM) fppmm $(BINDIR)/fppmm
	strip fppd
	-$(INSTALL_PROGRAM) fppd $(BINDIR)/fppd
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
	@sh fppversion.sh $(PWD)

%.o: %.cpp %.h Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

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

util: force
	$(MAKE) -C util

restart: fppd
	sudo ../scripts/fppd_stop
	sudo ../scripts/fppd_start

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
	@make -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a

../lib/librgbmatrix.so: ../external/rpi-rgb-led-matrix/.git
	@echo "Building rpi-rgb-led-matrix library for Standard pinout"
	@make -C ../external/rpi-rgb-led-matrix/lib/ clean
	@make -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a \
		DEFINES='-DRGB_SLOWDOWN_GPIO=1'
	gcc -shared -o ../lib/librgbmatrix_Standard.so ../external/rpi-rgb-led-matrix/lib/*.o
	@echo "Building rpi-rgb-led-matrix library for Adafruit pinout"
	@make -C ../external/rpi-rgb-led-matrix/lib/ clean
	@make -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a \
		DEFINES='-DRGB_SLOWDOWN_GPIO=1 -DADAFRUIT_RGBMATRIX_HAT -DONLY_SINGLE_CHAIN'
	gcc -shared -o ../lib/librgbmatrix_Adafruit.so ../external/rpi-rgb-led-matrix/lib/*.o
	@echo "Building rpi-rgb-led-matrix library for Classic pinout"
	@make -C ../external/rpi-rgb-led-matrix/lib/ clean
	@make -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a \
		DEFINES='-DRGB_SLOWDOWN_GPIO=1 -DRGB_CLASSIC_PINOUT -DONLY_SINGLE_CHAIN'
	gcc -shared -o ../lib/librgbmatrix_Classic.so ../external/rpi-rgb-led-matrix/lib/*.o
	strip ../lib/librgbmatrix_Standard.so ../lib/librgbmatrix_Adafruit.so ../lib/librgbmatrix_Classic.so
	cp ../lib/librgbmatrix_Standard.so ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so
	ln -s ./librgbmatrix_Standard.so ../lib/librgbmatrix.so


#############################################################################
# libws2811 on the Pi
../external/rpi_ws281x/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi_ws281x/libws2811.a: ../external/rpi_ws281x/.git
	@echo "Building libws2811"
	@cd ../external/rpi_ws281x/ && \
		gcc -c -o rpihw.o rpihw.c && \
		gcc -c -o mailbox.o mailbox.c && \
		gcc -c -o dma.o dma.c && \
		gcc -c -o pwm.o pwm.c && \
		gcc -c -o ws2811.o ws2811.c && \
		ar rcs libws2811.a rpihw.o mailbox.o dma.o pwm.o ws2811.o

#############################################################################
# LEDscape library and PRU code on the BBB
../external/LEDscape/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/LEDscape/lib/libledscape.a: ../external/LEDscape/.git  ../external/LEDscape/src/ledscape/ledscape.c ../external/LEDscape/src/ledscape/ledscape.h
	@echo "Building LEDscape library"
	@make -C ../external/LEDscape

