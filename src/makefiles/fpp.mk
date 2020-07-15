OBJECTS_fpp = \
	fpp.o \
	common.o \
    log.o \
	fppversion.o
LIBS_fpp = \
    -ljsoncpp \
    -lcurl \
    -lpthread -lrt \
	$(NULL)

TARGETS+=fpp
OBJECTS_ALL+=$(OBJECTS_fpp)

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

