

all: vc4-kms-dpi-fpp-pi3.dtbo vc4-kms-dpi-fpp-pi4.dtbo


vc4-kms-dpi-fpp-pi4.dtbo: vc4-kms-dpi-fpp.dts
	cpp -DCLOCK_FREQUENCY=7200000 $< | dtc -@ -I dts -O dtb -o $@ 

vc4-kms-dpi-fpp-pi3.dtbo: vc4-kms-dpi-fpp.dts
	cpp -DCLOCK_FREQUENCY=2400000 $< | dtc -@ -I dts -O dtb -o $@ 

install: vc4-kms-dpi-fpp-pi3.dtbo vc4-kms-dpi-fpp-pi4.dtbo
	cp -f vc4-kms-dpi-fpp-pi3.dtbo /boot/firmware/overlays
	cp -f vc4-kms-dpi-fpp-pi4.dtbo /boot/firmware/overlays


clean:
	rm -f  vc4-kms-dpi-fpp-pi3.dtbo vc4-kms-dpi-fpp-pi4.dtbo