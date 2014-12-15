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
	channeloutput/channeloutput.o \
	channeloutput/channeloutputthread.o \
	channeloutput/E131.o \
	channeloutput/FPD.o \
	channeloutput/GPIO.o \
	channeloutput/GPIO595.o \
	channeloutput/LOR.o \
	channeloutput/serialutil.o \
	channeloutput/SPInRF24L01.o \
	channeloutput/SPIws2801.o \
	channeloutput/Triks-C.o \
	channeloutput/USBDMXOpen.o \
	channeloutput/USBDMXPro.o \
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
	playList.o \
	plugins.o \
	schedule.o \
	sequence.o \
	settings.o \
	$(NULL)
LIBS_fppd = \
	-lpthread \
	-lm \
	-lutil \
	-ltag_c \
	$(NULL)


ARCH := $(shell cat /etc/fpp/platform 2> /dev/null)
ifeq '$(ARCH)' ''
	ARCH = "UNKNOWN"
endif
$(shell echo "Building FPP for '$(ARCH)' platform" 1>&2)

# Common CFLAGS
CFLAGS += \
	-I . \
	-I channeloutput \
	-I mediaoutput \
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
	-DUSEWIRINGPI \
	-DUSENRF \
	-I../external/RF24/ \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	-lwiringPiDev \
	-lrf24-bcm \
	$(NULL)
LDFLAGS_fppd += \
	-L../external/RF24 \
	$(NULL)
DEPS_fppd += \
	../external/RF24/librf24-bcm.so \
	$(NULL)
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

force:

release: $(TARGETS)
	strip fpp
	-$(INSTALL_PROGRAM) fpp ../bin/fpp
	strip fppmm
	-$(INSTALL_PROGRAM) fppmm ../bin/fppmm
	strip fppd
	-$(INSTALL_PROGRAM) fppd ../bin/fppd

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppmm: $(OBJECTS_fppmm)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppd: $(DEPS_fppd) $(OBJECTS_fppd)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppversion.c: fppversion.sh force
	sh fppversion.sh $(PWD)

%.o: %.c %.h Makefile
	$(CCACHE) $(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f fppversion.c $(OBJECTS_fpp) $(OBJECTS_fppmm) $(OBJECTS_fppd) fpp fppmm fppd
	@[ -e ../external/RF24/.git ] && make -C ../external/RF24 clean

../external/RF24/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/RF24/librf24-bcm.so: ../external/RF24/.git
	@echo "Building RF24 library"
	@make -C ../external/RF24/
	@ln -s librf24-bcm.so.1.0 ../external/RF24/librf24-bcm.so.1
	@ln -s librf24-bcm.so.1 ../external/RF24/librf24-bcm.so
