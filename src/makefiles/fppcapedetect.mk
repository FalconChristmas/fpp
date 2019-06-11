
OBJECTS_fppcapedetect = \
	fppcapedetect.o \
	CapeUtils.o

LIBS_fppcapedetect = \
	-ljsoncpp

# At boot time, reading off the SD card is a huge bottleneck as nothing is cached
# and all the services starting in parallel are trying to load things.
# Optimize for as small as possible
CXXFLAGS_CapeUtils.o=-Os -flto
CFLAGS_fppcapedetect.o=-Os -flto

TARGETS+=fppcapedetect
OBJECTS_ALL+=$(OBJECTS_fppcapedetect)

fppcapedetect: $(OBJECTS_fppcapedetect)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

