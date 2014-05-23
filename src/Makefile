CC := gcc

# Uncomment the following line to build with g++ for testing, etc.
#CC := g++

TARGETS = fpp fppmm fppd

INSTALL_PROGRAM = install -m 755 -p

OBJECTS_fpp = \
	fpp.o \
	$(NULL)
LIBS_fpp = \
	$(NULL)

OBJECTS_fppmm = \
	common.o \
	fppmm.o \
	$(NULL)
LIBS_fppmm = \
	$(NULL)

OBJECTS_fppd = \
	channeloutput/channeloutput.o \
	channeloutput/channeloutputthread.o \
	channeloutput/E131.o \
	channeloutput/FPD.o \
	channeloutput/serialutil.o \
	channeloutput/SPIws2801.o \
	channeloutput/USBDMXOpen.o \
	channeloutput/USBDMXPro.o \
	channeloutput/USBPixelnet.o \
	channeloutput/USBRenard.o \
	command.o \
	common.o \
	e131bridge.o \
	effects.o \
	events.o \
	fppd.o \
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


ARCH := $(shell uname -m)

# Common CFLAGS
CFLAGS += \
	-I . \
	-I channeloutput \
	-I mediaoutput \
	$(NULL)

# Do some things a little different when using gcc vs g++
ifeq ($(CC), gcc)
# Warn us if we forget to #include something, but this doesn't work on g++
CFLAGS += -Wimplicit
else
# we get lots of deprecated warnings currently with g++ since we're
# normally compiling with gcc
CFLAGS += -Wno-write-strings
endif

ifeq ($(ARCH), armv6l)
# do something RaspPi-y
CFLAGS += \
	-DUSEWIRINGPI \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	$(NULL)
else
ifeq ($(filter-out i686 x86_64 armv5tel, $(ARCH)),)
# do something Linux-y
CFLAGS += \
	-g \
	-DNOROOT \
	$(NULL)
else
	VAR := $(error Unsupported architecture type)
endif
endif


.PHONY: all
all: $(TARGETS)

release: $(TARGETS)
	strip fpp
	-$(INSTALL_PROGRAM) fpp ../bin/fpp
	strip fppmm
	-$(INSTALL_PROGRAM) fppmm ../bin/fppmm
	strip fppd
	-$(INSTALL_PROGRAM) fppd ../bin/fppd

fpp: $(OBJECTS_fpp)
	$(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppmm: $(OBJECTS_fppmm)
	$(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppd: $(OBJECTS_fppd)
	$(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

%.o: %.c %.h Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS_fpp) $(OBJECTS_fppmm) $(OBJECTS_fppd) fpp fppmm fppd
