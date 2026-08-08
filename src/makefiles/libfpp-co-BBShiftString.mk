# BeagleBone
ifeq ($(ISBEAGLEBONE), 1)

OBJECTS_fpp_co_BBShiftString_so += non-gpl/BBShiftString/BBShiftString.o
LIBS_fpp_co_BBShiftString_so += -L. -lfpp -ljsoncpp -lfpp_capeutils -lfpp-FalconV5Support -Wl,-rpath=$(SRCDIR):.

TARGETS += libfpp-co-BBShiftString.$(SHLIB_EXT) non-gpl/BBShiftString/BBShiftString_pru0.out non-gpl/BBShiftString/BBShiftString_pru1.out
ifeq '$(ARMV)' 'aarch64'
TARGETS += non-gpl/BBShiftString/BBShiftString_pru0_16.out non-gpl/BBShiftString/BBShiftString_pru1_16.out
endif
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBShiftString_so)

CXXFLAGS_non-gpl/BBShiftString/BBShiftString.o+=-Wno-address-of-packed-member

libfpp-co-BBShiftString.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBShiftString_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT) libfpp-FalconV5Support.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBShiftString_so) $(LIBS_fpp_co_BBShiftString_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBShiftString_so) -o $@




ifeq '$(ARMV)' 'aarch64'
non-gpl/BBShiftString/BBShiftString_pru1.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DRUNNING_ON_PRU1 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru1.asm"
	clpru -v3 -o -DAM62X -DRUNNING_ON_PRU1 --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftString_pru1.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftString/BBShiftString_pru1.out" "/tmp/BBShiftString_pru1.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftString_pru1.asm" "/tmp/BBShiftString_pru1.obj"

non-gpl/BBShiftString/BBShiftString_pru0.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DRUNNING_ON_PRU0 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru0.asm"
	clpru -v3 -o -DAM62X -DRUNNING_ON_PRU0 --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftString_pru0.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftString/BBShiftString_pru0.out" "/tmp/BBShiftString_pru0.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftString_pru0.asm" "/tmp/BBShiftString_pru0.obj"

# 16 deep shift register chains (128 strings on one PRU) get their own firmware
# rather than a runtime switch: the shift loop count, the T0 high time and the
# command table record layout all differ, and the 8 deep path has to stay
# exactly as it is for every cape shipped so far.  AM62x only - at the AM335x
# 200MHz the three shift phases total 1045ns against a 1120ns bit budget with
# no room left for the block load, and two PRUs already give 128 strings there.
non-gpl/BBShiftString/BBShiftString_pru1_16.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DRUNNING_ON_PRU1 -DSHIFT16 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru1_16.asm"
	clpru -v3 -o -DAM62X -DRUNNING_ON_PRU1 -DSHIFT16 --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftString_pru1_16.asm"
	clpru -v3 -DAM62X -DSHIFT16 -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftString/BBShiftString_pru1_16.out" "/tmp/BBShiftString_pru1_16.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftString_pru1_16.asm" "/tmp/BBShiftString_pru1_16.obj"

non-gpl/BBShiftString/BBShiftString_pru0_16.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DRUNNING_ON_PRU0 -DSHIFT16 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru0_16.asm"
	clpru -v3 -o -DAM62X -DRUNNING_ON_PRU0 -DSHIFT16 --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftString_pru0_16.asm"
	clpru -v3 -DAM62X -DSHIFT16 -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftString/BBShiftString_pru0_16.out" "/tmp/BBShiftString_pru0_16.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftString_pru0_16.asm" "/tmp/BBShiftString_pru0_16.obj"

# Nonstandard pinouts (e.g. a shared panels + strings cape) need no firmware
# variant: the cape names its pins in pruPinConfig, the C++ resolves them to
# r30 bit numbers via the platform pin table, the data bits map at prep time
# and the clock/latch bits are published to the firmware at runtime.  Only
# if a pinout ever forces the data bits out of r30.b0 would a variant with
# -DDATA_BYTE=r30.bN be needed here.

else

non-gpl/BBShiftString/BBShiftString_pru1.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM33XX -DRUNNING_ON_PRU1 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru1.asm"
	clpru -v3 -c --obj_directory /tmp -DAM33XX -DRUNNING_ON_PRU1 "/tmp/BBShiftString_pru1.asm"
	clpru -v3 -z --entry_point main pru/AM335x_PRU.cmd -o "non-gpl/BBShiftString/BBShiftString_pru1.out" "/tmp/BBShiftString_pru1.obj"
	@rm "/tmp/BBShiftString_pru1.asm" "/tmp/BBShiftString_pru1.obj"

non-gpl/BBShiftString/BBShiftString_pru0.out: non-gpl/BBShiftString/BBShiftString.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM33XX -DRUNNING_ON_PRU0 "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru0.asm"
	clpru -v3 -c --obj_directory /tmp -DAM33XX -DRUNNING_ON_PRU0 "/tmp/BBShiftString_pru0.asm"
	clpru -v3 -z --entry_point main pru/AM335x_PRU.cmd -o "non-gpl/BBShiftString/BBShiftString_pru0.out" "/tmp/BBShiftString_pru0.obj"
	@rm "/tmp/BBShiftString_pru0.asm" "/tmp/BBShiftString_pru0.obj"
endif


endif
