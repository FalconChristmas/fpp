all: BBB-AUDIO-PCM5012A-00A0.dtbo BBB-P8-AUDIO-PCM5012A-00A0.dtbo fpp-base-overlay.dtb fpp-cape-overlay.dtb

clean:
	rm -f BBB-AUDIO-PCM5012A-00A0.dtbo BBB-P8-AUDIO-PCM5012A-00A0.dtbo fpp-base-overlay.dtb fpp-cape-overlay.dtb

install:
	cp BBB-AUDIO-PCM5012A-00A0.dtbo /lib/firmware
	cp BBB-P8-AUDIO-PCM5012A-00A0.dtbo /lib/firmware
	cp fpp-base-overlay.dtb /lib/firmware
	cp fpp-cape-overlay.dtb /lib/firmware
	cp fpp-cape-overlay.dtb /lib/firmware/fpp-cape-overlay-default.dtb

BBB-AUDIO-PCM5012A-00A0.dtbo: BBB-AUDIO-PCM5012A-00A0.dts
	cp BBB-AUDIO-PCM5012A-00A0.dts /opt/source/bb.org-overlays/src/arm
	touch /opt/source/bb.org-overlays/src/arm/BBB-AUDIO-PCM5012A-00A0.dts
	cd /opt/source/bb.org-overlays ; make src/arm/BBB-AUDIO-PCM5012A-00A0.dtbo
	cp /opt/source/bb.org-overlays/src/arm/BBB-AUDIO-PCM5012A-00A0.dtbo .

BBB-P8-AUDIO-PCM5012A-00A0.dtbo: BBB-P8-AUDIO-PCM5012A-00A0.dts
	cp BBB-P8-AUDIO-PCM5012A-00A0.dts /opt/source/bb.org-overlays/src/arm
	touch /opt/source/bb.org-overlays/src/arm/BBB-P8-AUDIO-PCM5012A-00A0.dts
	cd /opt/source/bb.org-overlays ; make src/arm/BBB-P8-AUDIO-PCM5012A-00A0.dtbo
	cp /opt/source/bb.org-overlays/src/arm/BBB-P8-AUDIO-PCM5012A-00A0.dtbo .

fpp-base-overlay.dtb: fpp-base-overlay.dts
	cpp -I/opt/source/dtb-6.18.x/include/ -I/opt/source/dtb-6.18.x/src/arm/ti/omap -nostdinc -undef -x assembler-with-cpp fpp-base-overlay.dts > .fpp-base-overlay.dts
	dtc --include /opt/source/dtb-6.18.x/include/ --include /opt/source/dtb-6.18.x/src/arm/ti/omap -O dtb -o fpp-base-overlay.dtb .fpp-base-overlay.dts
	rm .fpp-base-overlay.dts

fpp-cape-overlay.dtb: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtb fpp-cape-overlay.dts
