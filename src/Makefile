CC := gcc

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
	mediaoutput/mediaoutput.o \
	mediaoutput/mpg123.o \
	mediaoutput/ogg123.o \
	mediaoutput/omxplayer.o \
	memorymap.o \
	playList.o \
	schedule.o \
	sequence.o \
	settings.o \
	$(NULL)
LIBS_fppd = \
	-lpthread \
	-lm \
	-lutil \
	$(NULL)


ARCH := $(shell uname -m)

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
	-$(INSTALL_PROGRAM) fpp ../bin/fpp
	-$(INSTALL_PROGRAM) fppmm ../bin/fppmm
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
