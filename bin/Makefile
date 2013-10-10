CC := gcc

TARGETS = fpp fppd

OBJECTS_fpp = \
	fpp.o \
	$(NULL)
LIBS_fpp = \
	$(NULL)

OBJECTS_fppd = \
	fppd.o \
	log.o \
	E131.o \
	ogg123.o \
	playList.o \
	schedule.o \
	command.o \
	e131bridge.o \
	settings.o \
	lightthread.o \
	$(NULL)
LIBS_fppd = \
	-lpthread \
	-lm \
	$(NULL)


ARCH := $(shell uname -m)

ifeq ($(ARCH), armv6l)
# do something RaspPi-y
OBJECTS_fppd += \
	pixelnetDMX.o \
	$(NULL)
LIBS_fppd += \
	-lwiringPi \
	-lm \
	$(NULL)
else
ifeq ($(filter-out i686 x86_64, $(ARCH)),)
# do something Linux-y
CFLAGS += \
	-g \
	-DNOROOT \
	$(NULL)
OBJECTS_fppd += \
	pixelnetStubs.o \
	$(NULL)
else
	VAR := $(error Unsupported architecture type)
endif
endif


.PHONY: all
all: $(TARGETS)

fpp: $(OBJECTS_fpp)
	$(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppd: $(OBJECTS_fppd)
	$(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

%.o: %.c %.h Makefile
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS_fpp) $(OBJECTS_fppd) fpp fppd
