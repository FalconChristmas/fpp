OBJECTS_fsequtils = \
	common.o \
	fppversion.o \
    log.o \
	Warnings.o \
    fseq/FSEQUtils.o \
    fseq/FSEQFile.o

LIBS_fsequtils = \
	-lcurl \
	-ljsoncpp \
    -lzstd -lz \
	-lpthread

TARGETS += fsequtils
OBJECTS_ALL+=$(OBJECTS_fsequtils)

fsequtils: $(OBJECTS_fsequtils)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

