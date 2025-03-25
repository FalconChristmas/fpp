# BeagleBone
ifeq ($(ISBEAGLEBONE), 1)

OBJECTS_fpp_co_BBShiftString_so += non-gpl/BBShiftString/BBShiftString.o
LIBS_fpp_co_BBShiftString_so += -L. -lfpp -ljsoncpp -lfpp_capeutils -lfpp-FalconV5Support -Wl,-rpath=$(SRCDIR):.

TARGETS += libfpp-co-BBShiftString.$(SHLIB_EXT) non-gpl/BBShiftString/BBShiftString_pru0.out non-gpl/BBShiftString/BBShiftString_pru1.out
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBShiftString_so)

CXXFLAGS_non-gpl/BBShiftString/BBShiftString.o+=-Wno-address-of-packed-member

libfpp-co-BBShiftString.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBShiftString_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT) libfpp-FalconV5Support.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBShiftString_so) $(LIBS_fpp_co_BBShiftString_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBShiftString_so) -o $@




ifeq '$(ARMV)' 'aarch64'
non-gpl/BBShiftString/BBShiftString_pru1.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DRUNNING_ON_PRU1 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString.asm"
	clpru -v3 -o -DAM62X -DRUNNING_ON_PRU1 --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftString.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftString/BBShiftString_pru1.out" "/tmp/BBShiftString.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftString.asm" "/tmp/BBShiftString.obj"

non-gpl/BBShiftString/BBShiftString_pru0.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DRUNNING_ON_PRU0 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString.asm"
	clpru -v3 -o -DAM62X -DRUNNING_ON_PRU0 --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftString.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftString/BBShiftString_pru0.out" "/tmp/BBShiftString.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftString.asm" "/tmp/BBShiftString.obj"

else

non-gpl/BBShiftString/BBShiftString_pru1.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM33XX -DRUNNING_ON_PRU1 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString.asm"
	clpru -v3 -c --obj_directory /tmp -DAM33XX -DRUNNING_ON_PRU1 /tmp/BBShiftString.asm"
	clpru -v3 -z --entry_point main pru/AM335x_PRU.cmd -o "non-gpl/BBShiftString/BBShiftString_pru1.out" "/tmp/BBShiftString.obj"
	@rm "/tmp/BBShiftString.asm" "/tmp/BBShiftString.obj"

non-gpl/BBShiftString/BBShiftString_pru0.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM33XX -DRUNNING_ON_PRU0 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString.asm"
	clpru -v3 -c --obj_directory /tmp -DAM33XX -DRUNNING_ON_PRU0 /tmp/BBShiftString.asm"
	clpru -v3 -z --entry_point main pru/AM335x_PRU.cmd -o "non-gpl/BBShiftString/BBShiftString_pru0.out" "/tmp/BBShiftString.obj"
	@rm "/tmp/BBShiftString.asm" "/tmp/BBShiftString.obj"
endif


endif
