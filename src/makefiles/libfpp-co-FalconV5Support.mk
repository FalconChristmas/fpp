# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

OBJECTS_fpp_FalconV5Support_so += non-gpl/FalconV5Support/FalconV5Support.o
LIBS_fpp_FalconV5Support_so += -L. -lfpp -ljsoncpp -lFalconV5 -Wl,-rpath=$(SRCDIR):.

TARGETS += libFalconV5.$(SHLIB_EXT) libfpp-FalconV5Support.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_FalconV5Support_so)

libFalconV5.$(SHLIB_EXT): non-gpl/FalconV5Support/libFalconV5.$(SHLIB_EXT).zst
	zstd -fd non-gpl/FalconV5Support/libFalconV5.$(SHLIB_EXT).zst -o libFalconV5.$(SHLIB_EXT)

libfpp-FalconV5Support.$(SHLIB_EXT): $(OBJECTS_fpp_FalconV5Support_so) libfpp.$(SHLIB_EXT) libFalconV5.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_FalconV5Support_so) $(LIBS_fpp_FalconV5Support_so) $(LDFLAGS) $(LDFLAGS_fpp_FalconV5Support_so) -o $@


endif
