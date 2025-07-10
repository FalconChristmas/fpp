
all: vc4-kms-dpi-fpp-pi3.dtbo vc4-kms-dpi-fpp-pi4.dtbo fpp-cape-overlay.dtbo


vc4-kms-dpi-fpp-pi4.dtbo: vc4-kms-dpi-fpp.dts
	cpp -DCLOCK_FREQUENCY=7200000 $< | dtc -@ -I dts -O dtb -o $@ 

vc4-kms-dpi-fpp-pi3.dtbo: vc4-kms-dpi-fpp.dts
	cpp -DCLOCK_FREQUENCY=2400000 $< | dtc -@ -I dts -O dtb -o $@ 

fpp-cape-overlay.dtbo: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtbo fpp-cape-overlay.dts

install: vc4-kms-dpi-fpp-pi3.dtbo vc4-kms-dpi-fpp-pi4.dtbo fpp-cape-overlay.dtbo
	cp -f fpp-cape-overlay.dtbo /boot/firmware/overlays
	cp -f fpp-cape-overlay.dtbo /boot/firmware/overlays/fpp-cape-overlay-default.dtbo
	cp -f vc4-kms-dpi-fpp-pi3.dtbo /boot/firmware/overlays
	cp -f vc4-kms-dpi-fpp-pi4.dtbo /boot/firmware/overlays


clean:
	rm -f  vc4-kms-dpi-fpp-pi3.dtbo vc4-kms-dpi-fpp-pi4.dtbo fpp-cape-overlay.dtbo