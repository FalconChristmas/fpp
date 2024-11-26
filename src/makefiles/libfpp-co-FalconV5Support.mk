# BeagleBone
ifeq ($(ISBEAGLEBONE), 1)

OBJECTS_fpp_FalconV5Support_so += non-gpl/FalconV5Support/FalconV5Support.o
LIBS_fpp_FalconV5Support_so += -L. -lfpp -ljsoncpp -lFalconV5 -Wl,-rpath=$(SRCDIR):.

TARGETS += libFalconV5.$(SHLIB_EXT) libfpp-FalconV5Support.$(SHLIB_EXT) non-gpl/FalconV5Support/FalconV5PRUListener_pru0.out
OBJECTS_ALL+=$(OBJECTS_fpp_FalconV5Support_so)

ifeq '$(ARMV)' 'aarch64'
ZEXT=.aarch64
else
ZEXT=
endif

libFalconV5.$(SHLIB_EXT): non-gpl/FalconV5Support/libFalconV5.$(SHLIB_EXT)$(ZEXT).zst
	zstd -fd non-gpl/FalconV5Support/libFalconV5.$(SHLIB_EXT)$(ZEXT).zst -o libFalconV5.$(SHLIB_EXT)

libfpp-FalconV5Support.$(SHLIB_EXT): $(OBJECTS_fpp_FalconV5Support_so) libfpp.$(SHLIB_EXT) libFalconV5.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_FalconV5Support_so) $(LIBS_fpp_FalconV5Support_so) $(LDFLAGS) $(LDFLAGS_fpp_FalconV5Support_so) -o $@

non-gpl/FalconV5Support/FalconV5PRUListener_pru0.out: non-gpl/FalconV5Support/FalconV5PRUListener.asm
	/usr/bin/cpp -P -Ipru "non-gpl/FalconV5Support/FalconV5PRUListener.asm" > "/tmp/FalconV5PRUListener_pru0.asm"
	clpru -v3 -c --obj_directory /tmp "/tmp/FalconV5PRUListener_pru0.asm"
	clpru -v3 -z --entry_point main pru/AM335x_PRU.cmd -o "non-gpl/FalconV5Support/FalconV5PRUListener_pru0.out" "/tmp/FalconV5PRUListener_pru0.obj"

endif
