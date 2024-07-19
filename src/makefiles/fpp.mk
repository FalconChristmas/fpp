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

ifneq ($(wildcard /usr/include/kms++/kms++.h),)
LIBS_fpp += -lkms++ -lkms++util
endif

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

