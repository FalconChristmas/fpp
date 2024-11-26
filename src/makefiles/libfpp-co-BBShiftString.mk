# BeagleBone
ifeq ($(ISBEAGLEBONE), 1)

OBJECTS_fpp_co_BBShiftString_so += non-gpl/BBShiftString/BBShiftString.o
LIBS_fpp_co_BBShiftString_so += -L. -lfpp -ljsoncpp -lfpp_capeutils -lfpp-FalconV5Support -Wl,-rpath=$(SRCDIR):.

TARGETS += libfpp-co-BBShiftString.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBShiftString_so)

CXXFLAGS_non-gpl/BBShiftString/BBShiftString.o+=-Wno-address-of-packed-member

libfpp-co-BBShiftString.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBShiftString_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT) libfpp-FalconV5Support.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBShiftString_so) $(LIBS_fpp_co_BBShiftString_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBShiftString_so) -o $@


endif
