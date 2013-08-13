CC := gcc

LIBS  = 
CFLAGS = -I.
LDFLAGS =


TARGETS = fpp fppd

OBJECTS_fpp = \
	fpp.o \
	$(NULL)
LIBS_fpp = \
	$(NULL)

OBJECTS_fppd = \
	fppd.o \
	E131.o \
	mpg123.o \
	pixelnetDMX.o \
	playList.o \
	schedule.o \
	command.o \
	e131bridge.o \
	$(NULL)
LIBS_fppd = \
	-lpthread \
	-lwiringPi \
	$(NULL)


.PHONY: all
all: $(TARGETS)

fpp: $(OBJECTS_fpp)
	$(CC) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

fppd: $(OBJECTS_fppd)
	$(CC) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS_$@) -o $@

%.o: %.c %.h Makefile
	$(CC) -c $<  

clean:
	rm -f $(OBJECTS_fpp) $(OBJECTS_fppd)
	-git co fpp fppd
