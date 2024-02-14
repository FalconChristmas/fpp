OBJECTS_fsequtils = \
	common.o \
	common_mini.o \
	fppversion.o \
    log.o \
	Warnings.o \
    fseq/FSEQUtils.o \
    fseq/FSEQFile.o

ifeq '$(ARCH)' 'OSX'
LIBS_fsequtils = \
	-lcurl \
	-ljsoncpp \
    -lz $(HOMEBREW)/opt/zstd/lib/libzstd.a
else
LIBS_fsequtils = \
	-lcurl \
	-ljsoncpp \
    -lzstd -lz
endif

TARGETS += fsequtils
OBJECTS_ALL+=$(OBJECTS_fsequtils)

fsequtils: $(OBJECTS_fsequtils) $(PCH_FILE)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

fseq/%.o: fseq/%.cpp fseq/%.h fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

ifeq '$(ARCH)' 'OSX'
OBJECTS_ALL+=libjsoncpp.25.dylib fsequtils.zip
fsequtils.zip: fsequtils
	cp $(HOMEBREW)/opt/jsoncpp/lib/libjsoncpp.25.dylib .
	chmod +w libjsoncpp.25.dylib
	install_name_tool -change $(HOMEBREW)/opt/jsoncpp/lib/libjsoncpp.25.dylib ./libjsoncpp.25.dylib fsequtils
	zip -9 fsequtils.zip fsequtils libjsoncpp.25.dylib
endif
