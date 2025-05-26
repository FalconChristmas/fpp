# BeagleBone
ifeq '$(ARCH)' 'BeagleBone 64'

OBJECTS_fpp_co_BBShiftPanel_so += non-gpl/BBShiftPanel/BBShiftPanel.o non-gpl/BBShiftPanel/MapPixelsByDepth16.o

LIBS_fpp_co_BBShiftPanel_so += -L. -lfpp -ljsoncpp -lfpp_capeutils -Wl,-rpath=$(SRCDIR):.

TARGETS += libfpp-co-matrix-BBShiftPanel.$(SHLIB_EXT) non-gpl/BBShiftPanel/BBShiftPanel.out non-gpl/BBShiftPanel/BBShiftPanel_single.out non-gpl/BBShiftPanel/BBShiftPanel_pwm.out non-gpl/BBShiftPanel/BBShiftPanel_oe.out non-gpl/BBShiftPanel/BBShiftPanel_gclk.out
OBJECTS_ALL+=$(OBJECTS_fpp_co_BBShiftPanel_so)

CXXFLAGS_non-gpl/BBShiftPanel/BBShiftPanel.o+=-Wno-address-of-packed-member

non-gpl/BBShiftPanel/MapPixelsByDepth16.o: non-gpl/BBShiftPanel/MapPixelsByDepth16.o_ispc
	@cp -f non-gpl/BBShiftPanel/MapPixelsByDepth16.o_ispc non-gpl/BBShiftPanel/MapPixelsByDepth16.o

libfpp-co-matrix-BBShiftPanel.$(SHLIB_EXT): $(OBJECTS_fpp_co_BBShiftPanel_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_BBShiftPanel_so) $(LIBS_fpp_co_BBShiftPanel_so) $(LDFLAGS) $(LDFLAGS_fpp_co_BBShiftPanel_so) -o $@

non-gpl/BBShiftPanel/BBShiftPanel_single.out: non-gpl/BBShiftPanel/BBShiftPanel.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X -DSINGLEPRU "/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel.asm" > "/tmp/BBShiftPanel_single.asm"
	clpru -v3 -o -DAM62X -DSINGLEPRU --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftPanel_single.asm"
	clpru -v3 -DAM62X -DSINGLEPRU -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftPanel/BBShiftPanel_single.out" "/tmp/BBShiftPanel_single.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftPanel_single.asm" "/tmp/BBShiftPanel_single.obj"

non-gpl/BBShiftPanel/BBShiftPanel.out: non-gpl/BBShiftPanel/BBShiftPanel.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X "/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel.asm" > "/tmp/BBShiftPanel.asm"
	clpru -v3 -o -DAM62X --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftPanel.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftPanel/BBShiftPanel.out" "/tmp/BBShiftPanel.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftPanel.asm" "/tmp/BBShiftPanel.obj"

non-gpl/BBShiftPanel/BBShiftPanel_pwm.out: non-gpl/BBShiftPanel/BBShiftPanel_pwm.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X "/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_pwm.asm" > "/tmp/BBShiftPanel_pwm.asm"
	clpru -v3 -o -DAM62X --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftPanel_pwm.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftPanel/BBShiftPanel_pwm.out" "/tmp/BBShiftPanel_pwm.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftPanel_pwm.asm" "/tmp/BBShiftPanel_pwm.obj"

non-gpl/BBShiftPanel/BBShiftPanel_oe.out: non-gpl/BBShiftPanel/BBShiftPanel_oe.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X "/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_oe.asm" > "/tmp/BBShiftPanel_oe.asm"
	clpru -v3 -o -DAM62X --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftPanel_oe.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftPanel/BBShiftPanel_oe.out" "/tmp/BBShiftPanel_oe.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftPanel_oe.asm" "/tmp/BBShiftPanel_oe.obj"

non-gpl/BBShiftPanel/BBShiftPanel_gclk.out: non-gpl/BBShiftPanel/BBShiftPanel_gclk.asm
	/usr/bin/cpp -P -I/opt/fpp/src/pru -DAM62X "/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_gclk.asm" > "/tmp/BBShiftPanel_gclk.asm"
	clpru -v3 -o -DAM62X --endian=little --hardware_mac=on --obj_directory /tmp "/tmp/BBShiftPanel_gclk.asm"
	clpru -v3 -DAM62X -z /opt/fpp/src/pru/AM62x_PRU0.cmd -o "non-gpl/BBShiftPanel/BBShiftPanel_gclk.out" "/tmp/BBShiftPanel_gclk.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	@rm "/tmp/BBShiftPanel_gclk.asm" "/tmp/BBShiftPanel_gclk.obj"
endif
