
all: vc4-kms-dpi-fpp.dtbo fpp-cape-overlay.dtbo fpp-uart0-nopins.dtbo

vc4-kms-dpi-fpp.dtbo: vc4-kms-dpi-fpp.dts
	cpp -DCLOCK_FREQUENCY=38400000 $< | dtc -@ -I dts -O dtb -o $@

fpp-cape-overlay.dtbo: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtbo fpp-cape-overlay.dts

fpp-uart0-nopins.dtbo: fpp-uart0-nopins.dts
	dtc -O dtb -o fpp-uart0-nopins.dtbo fpp-uart0-nopins.dts

install: vc4-kms-dpi-fpp.dtbo fpp-cape-overlay.dtbo fpp-uart0-nopins.dtbo
	cp -f fpp-cape-overlay.dtbo /boot/firmware/overlays
	cp -f fpp-cape-overlay.dtbo /boot/firmware/overlays/fpp-cape-overlay-default.dtbo
	cp -f vc4-kms-dpi-fpp.dtbo /boot/firmware/overlays
	cp -f fpp-uart0-nopins.dtbo /boot/firmware/overlays

clean:
	rm -f vc4-kms-dpi-fpp.dtbo fpp-cape-overlay.dtbo fpp-uart0-nopins.dtbo
