
DTD_BASE := /opt/source/dtb-6.18.x

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
	cpp -I$(DTD_BASE)/include/ -I$(DTD_BASE)/src/arm/ti/omap -nostdinc -undef -x assembler-with-cpp BBB-AUDIO-PCM5012A-00A0.dts > .BBB-AUDIO-PCM5012A-00A0.dts
	dtc --include $(DTD_BASE)/include/ --include $(DTD_BASE)/src/arm/ti/omap -O dtb -o BBB-AUDIO-PCM5012A-00A0.dtbo .BBB-AUDIO-PCM5012A-00A0.dts
	rm .BBB-AUDIO-PCM5012A-00A0.dts

BBB-P8-AUDIO-PCM5012A-00A0.dtbo: BBB-P8-AUDIO-PCM5012A-00A0.dts
	cpp -I$(DTD_BASE)/include/ -I$(DTD_BASE)/src/arm/ti/omap -nostdinc -undef -x assembler-with-cpp BBB-P8-AUDIO-PCM5012A-00A0.dts > .BBB-P8-AUDIO-PCM5012A-00A0.dts
	dtc --include $(DTD_BASE)/include/ --include $(DTD_BASE)/src/arm/ti/omap -O dtb -o BBB-P8-AUDIO-PCM5012A-00A0.dtbo .BBB-P8-AUDIO-PCM5012A-00A0.dts
	rm .BBB-P8-AUDIO-PCM5012A-00A0.dts

fpp-base-overlay.dtb: fpp-base-overlay.dts
	cpp -I$(DTD_BASE)/include/ -I$(DTD_BASE)/src/arm/ti/omap -nostdinc -undef -x assembler-with-cpp fpp-base-overlay.dts > .fpp-base-overlay.dts
	dtc --include $(DTD_BASE)/include/ --include $(DTD_BASE)/src/arm/ti/omap -O dtb -o fpp-base-overlay.dtb .fpp-base-overlay.dts
	rm .fpp-base-overlay.dts

fpp-cape-overlay.dtb: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtb fpp-cape-overlay.dts
