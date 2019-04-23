CC := g++

BASEDIR := $(shell basename `pwd`)

SRCDIRS := channeloutput channeloutput/processors channeltester fseq mediaoutput oled playlist pru sensors util

SRCDIR = ./
ifneq '$(BASEDIR)' 'src'
	VPATH = ../src/
	SRCDIR = ../src/
	MKDIROUT = $(shell mkdir $(SRCDIRS))
endif

ifneq ($(wildcard /usr/bin/ccache),)
	CCACHE = ccache
endif

TARGETS = fpp fppmm fsequtils fppd fppoled
SUBMODULES =

INSTALL_PROGRAM = install -m 755 -p

OBJECTS_fsequtils = \
	fppversion.o \
	log.o \
    fseq/FSEQUtils.o \
    fseq/FSEQFile.o \
	$(NULL)
LIBS_fsequtils = \
    -lzstd -lz \
	-lpthread \
	$(NULL)

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
	-ljsoncpp \
	$(NULL)


OBJECTS_fppoled = \
	common.o \
	log.o \
	settings.o \
	fppversion.o \
	oled/I2C.o \
	oled/SSD1306_OLED.o \
	oled/FPPOLEDUtils.o \
	oled/OLEDPages.o \
	oled/FPPStatusOLEDPage.o \
	oled/FPPMainMenu.o \
    oled/fppoled.o

LIBS_fppoled = \
	-ljsoncpp \
	-lpthread \
	-lcurl \
	-lgpiod \
	$(NULL)

OBJECTS_fppd = \
	channeloutput/ChannelOutputBase.o \
	channeloutput/ThreadedChannelOutputBase.o \
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
	channeloutput/HTTPVirtualDisplay.o \
	channeloutput/UDPOutput.o \
	channeloutput/Linsn-RV9.o \
	channeloutput/LOR.o \
	channeloutput/Matrix.o \
	channeloutput/MAX7219Matrix.o \
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
    channeloutput/processors/OutputProcessor.o \
    channeloutput/processors/RemapOutputProcessor.o \
    channeloutput/processors/SetValueOutputProcessor.o \
    channeloutput/processors/BrightnessOutputProcessor.o \
    channeloutput/processors/ColorOrderOutputProcessor.o \
	channeltester/ChannelTester.o \
	channeltester/TestPatternBase.o \
	channeltester/RGBChase.o \
	channeltester/RGBFill.o \
	channeltester/SingleChase.o \
	command.o \
	common.o \
	e131bridge.o \
	effects.o \
	events.o \
	falcon.o \
	fppd.o \
	fppversion.o \
	FrameBuffer.o \
	fseq/FSEQFile.o \
	gpio.o \
	httpAPI.o \
	log.o \
	MultiSync.o \
	mediadetails.o \
	mediaoutput/MediaOutputBase.o \
	mediaoutput/mediaoutput.o \
	mediaoutput/mpg123.o \
	mediaoutput/ogg123.o \
	mediaoutput/omxplayer.o \
	mediaoutput/SDLOut.o \
	mqtt.o \
	ping.o \
	PixelOverlay.o \
	Playlist.o \
	playlist/Playlist.o \
	playlist/PlaylistEntryBase.o \
	playlist/PlaylistEntryBoth.o \
	playlist/PlaylistEntryBranch.o \
	playlist/PlaylistEntryBrightness.o \
	playlist/PlaylistEntryChannelTest.o \
	playlist/PlaylistEntryDynamic.o \
	playlist/PlaylistEntryEffect.o \
	playlist/PlaylistEntryEvent.o \
	playlist/PlaylistEntryImage.o \
	playlist/PlaylistEntryMedia.o \
	playlist/PlaylistEntryMQTT.o \
	playlist/PlaylistEntryPause.o \
	playlist/PlaylistEntryPixelOverlay.o \
	playlist/PlaylistEntryPlaylist.o \
	playlist/PlaylistEntryPlugin.o \
	playlist/PlaylistEntryRemap.o \
	playlist/PlaylistEntryScript.o \
	playlist/PlaylistEntrySequence.o \
	playlist/PlaylistEntryURL.o \
	playlist/PlaylistEntryVolume.o \
	Plugins.o \
	Scheduler.o \
	ScheduleEntry.o \
	scripts.o \
	sensors/Sensors.o \
	Sequence.o \
	settings.o \
	SunSet.o \
	$(NULL)
LIBS_fppd = \
	-lboost_filesystem \
	-lboost_system \
	-lpthread \
    -lzstd -lz \
	-lhttpserver \
	-ljsoncpp \
	-lm \
	-lmosquitto \
	-lutil \
	-ltag \
	-lSDL2 \
	-lavformat \
	-lavcodec \
	-lavutil \
	-lswresample \
	-lswscale \
	$(NULL)

SUBDIRS = 

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

CURLFLAGS := $(shell curl-config --libs)

# Common CFLAGS
CFLAGS += \
	-I $(SRCDIR) \
	-I $(SRCDIR)channeloutput \
	-I $(SRCDIR)mediaoutput \
	$(NULL)
CXXFLAGS += -std=gnu++14

CXXFLAGS += $(shell GraphicsMagick++-config --cppflags)
LDFLAGS_fppd += -rdynamic $(CURLFLAGS) \
	$(shell GraphicsMagick++-config --ldflags --libs) \
	$(shell GraphicsMagickWand-config --ldflags --libs) \
	$(NULL)

DEPS_fppd += \
	$(NULL)


ifeq '$(ARCH)' 'Raspberry Pi'
# do something RaspPi-y
CFLAGS += \
	-DPLATFORM_PI \
	-DUSEWIRINGPI \
	-DUSENRF \
	-I../external/RF24/ \
	-I../external/rpi-rgb-led-matrix/include/ \
	-I../external/rpi_ws281x/ \
	-I../external/spixels/include/ \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	-lwiringPiDev \
	-lrf24-bcm \
	-lrgbmatrix \
	-lws2811 \
	-lspixels \
	$(NULL)
LIBS_fppoled += \
	-lwiringPi
LDFLAGS_fppd += \
	-L../external/RF24/ \
	-L../external/rpi-rgb-led-matrix/lib/ \
	-L../external/rpi_ws281x/ \
	-L../external/spixels/lib/ \
	$(NULL)
OBJECTS_fppd += \
	channeloutput/Hill320.o \
	channeloutput/ILI9488.o \
	channeloutput/RGBMatrix.o \
	channeloutput/rpi_ws281x.o \
	channeloutput/spixels.o \
	$(NULL)
DEPS_fppd += \
	../external/RF24/librf24-bcm.so \
	../external/rpi-rgb-led-matrix/lib/librgbmatrix.a \
	../external/rpi_ws281x/libws2811.a \
	../external/spixels/lib/libspixels.a \
	$(NULL)
SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels \
	$(NULL)
else ifeq '$(ARCH)' 'ODROID'
# do something for ODROID
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
	../external/rpi-rgb-led-matrix/lib/librgbmatrix.a \
	$(NULL)
SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	$(NULL)
else ifeq '$(ARCH)' 'OrangePi'
# do something for Orange Pi
CFLAGS += \
	-DPLATFORM_ORANGEPI \
	-DUSEWIRINGPI \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	-lwiringPiDev \
	$(NULL)
OBJECTS_fppd += \
	channeloutput/Hill320.o \
	$(NULL)
else ifeq '$(ARCH)' 'BeagleBone Black'
# BeagleBone Black
CFLAGS += \
	-DPLATFORM_BBB \
    -I/usr/local/include
LIBS_fppd += \
	-lprussdrv
LDFLAGS_fppd += \
    -L/usr/local/lib/
OBJECTS_fppd += \
	channeloutput/BBB48String.o \
	channeloutput/BBBSerial.o \
    channeloutput/BBBUtils.o \
    channeloutput/BBBPruUtils.o \
    channeloutput/BBBMatrix.o
OBJECTS_fppoled += channeloutput/BBBUtils.o
DEPS_fppd += \
	$(NULL)
SUBMODULES += \
	$(NULL)
else
# do something Linux-y
CFLAGS += \
	-DPLATFORM_UNKNOWN \
	$(NULL)
LDFLAGS_fppd += \
    -L. \
	$(NULL)
LDFLAGS_fsequtils += \
    -L. \
    $(NULL)
endif

##############################################################################
# Include a local Makefile if one exists to modify any of the above
-include Makefile.local
#
# Examples of some entries that might exist in Makefile.local:
#
# LIBS_fppd += -lX11
# CFLAGS += -DUSE_X11
# OBJECTS_fppd += channeloutput/X11Matrix.o channeloutput/X11VirtualDisplay.o
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

debug: $(TARGETS)
debug: CFLAGS+=-g

optimized: CFLAGS+=-O1
optimized: $(TARGETS)

release: optimized
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir release || exit 1; \
	done

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppmm: $(OBJECTS_fppmm)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppd: $(DEPS_fppd) $(OBJECTS_fppd)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fsequtils: $(OBJECTS_fsequtils)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppoled: $(OBJECTS_fppoled)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppversion.c: fppversion.sh force
	@sh $(SRCDIR)fppversion.sh $(PWD)

%.o: %.cpp %.h Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.c %.h Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

cleanfpp:
	rm -f fppversion.c $(OBJECTS_fpp) $(OBJECTS_fppmm) $(OBJECTS_fppd) $(OBJECTS_fppoled) $(OBJECTS_fsequtils) $(TARGETS)

clean:
	rm -f fppversion.c $(OBJECTS_fpp) $(OBJECTS_fppmm) $(OBJECTS_fppd) $(OBJECTS_fppoled) $(OBJECTS_fsequtils) $(TARGETS)
	@if [ -e ../external/RF24/.git ]; then make -C ../external/RF24 clean; fi
	@if [ -e ../external/rpi-rgb-led-matrix/.git ]; then make -C ../external/rpi-rgb-led-matrix clean; fi
	@if [ -e ../external/rpi_ws281x/libws2811.a ]; then rm ../external/rpi_ws281x/*.o ../external/rpi_ws281x/*.a 2> /dev/null; fi
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
	@CC="ccache gcc" CXX="ccache g++" make -C ../external/RF24/
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
	@if [ -e ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so ]; then rm ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so; fi
	@CC="ccache gcc" CXX="ccache g++" make -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a

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
		gcc -c -o pcm.o pcm.c && \
		gcc -c -o pwm.o pwm.c && \
		gcc -c -o ws2811.o ws2811.c && \
		ar rcs libws2811.a rpihw.o mailbox.o dma.o pwm.o pcm.o ws2811.o

#############################################################################
# spixels library on the Pi
../external/spixels/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/spixels/lib/libspixels.a: ../external/spixels/.git
	@echo "Building spixels library"
	@CC="ccache gcc" CXX="ccache g++" make -C ../external/spixels/lib/

