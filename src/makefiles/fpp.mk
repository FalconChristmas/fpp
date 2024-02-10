OBJECTS_fpp = \
	fpp.o \
	common.o \
    log.o \
	settings.o \
	common_mini.o \
	fppversion.o
LIBS_fpp = \
    -ljsoncpp \
    -lcurl \
	$(NULL)

TARGETS+=fpp
OBJECTS_ALL+=$(OBJECTS_fpp)

ifneq ($(wildcard /usr/include/libdrm/drm.h),)
CXXFLAGS_fpp.o += -I/usr/include/libdrm
LIBS_fpp += -ldrm
endif

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

