OBJECTS_fpp = \
	fpp.o \
	fppversion.o
LIBS_fpp = \
	$(NULL)

TARGETS+=fpp
OBJECTS_ALL+=$(OBJECTS_fpp)

fpp: $(OBJECTS_fpp)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

