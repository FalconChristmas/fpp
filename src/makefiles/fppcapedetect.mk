
OBJECTS_fppcapedetect = \
	fppcapedetect.o \
	CapeUtils.o

LIBS_fppcapedetect = \
	-ljsoncpp
    
ifeq '$(CXXCOMPILER)' 'g++'
# At boot time, reading off the SD card is a huge bottleneck as nothing is cached
# and all the services starting in parallel are trying to load things.
# Optimize for as small as possible
CXXFLAGS_CapeUtils.o=-Os -flto
CFLAGS_fppcapedetect.o=-Os -flto
else
CXXFLAGS_CapeUtils.o=-Os
CFLAGS_fppcapedetect.o=-Os
endif

TARGETS+=fppcapedetect
OBJECTS_ALL+=$(OBJECTS_fppcapedetect)

fppcapedetect: $(OBJECTS_fppcapedetect)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@


CapeUtils.o: CapeUtils.cpp CapeUtils.h fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

fppcapedetect.o: fppcapedetect.c fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@
