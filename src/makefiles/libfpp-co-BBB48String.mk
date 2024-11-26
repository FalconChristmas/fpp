# BeagleBone
ifeq ($(ISBEAGLEBONE), 1)

OBJECTS_fpp_co_BBB48String_so += non-gpl/BBB48String/BBB48String.o
LIBS_fpp_co_BBB48String_so += -L. -lfpp -ljsoncpp -lfpp_capeutils -Wl,-rpath=$(SRCDIR):.

TARGETS += libfpp-co-BBB48String.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBB48String_so)

CXXFLAGS_non-gpl/BBB48String/BBB48String.o+=-Wno-address-of-packed-member

libfpp-co-BBB48String.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBB48String_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBB48String_so) $(LIBS_fpp_co_BBB48String_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBB48String_so) -o $@


endif
