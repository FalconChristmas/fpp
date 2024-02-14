OBJECTS_fppmm = \
	common.o common_mini.o \
	log.o \
	fppmm.o \
	fppversion.o
LIBS_fppmm = \
	-lcurl \
	-ljsoncpp

TARGETS += fppmm
OBJECTS_ALL+=$(OBJECTS_fppmm)

fppmm: $(OBJECTS_fppmm)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

