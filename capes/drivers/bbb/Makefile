all: BBB-AUDIO-PCM5012A-00A0.dtbo bbb-fpp-reserve-memory.dtbo

clean:
	rm -f BBB-AUDIO-PCM5012A-00A0.dtbo  bbb-fpp-reserve-memory.dtbo

install:
	cp BBB-AUDIO-PCM5012A-00A0.dtbo /lib/firmware
	cp bbb-fpp-reserve-memory.dtbo /lib/firmware

BBB-AUDIO-PCM5012A-00A0.dtbo: BBB-AUDIO-PCM5012A-00A0.dts
	cp BBB-AUDIO-PCM5012A-00A0.dts /opt/source/bb.org-overlays/src/arm
	touch /opt/source/bb.org-overlays/src/arm/BBB-AUDIO-PCM5012A-00A0.dts
	cd /opt/source/bb.org-overlays ; make src/arm/BBB-AUDIO-PCM5012A-00A0.dtbo
	cp /opt/source/bb.org-overlays/src/arm/BBB-AUDIO-PCM5012A-00A0.dtbo .

bbb-fpp-reserve-memory.dtbo: bbb-fpp-reserve-memory.dts
	cp bbb-fpp-reserve-memory.dts /opt/source/bb.org-overlays/src/arm
	touch /opt/source/bb.org-overlays/src/arm/bbb-fpp-reserve-memory.dts
	cd /opt/source/bb.org-overlays ; make src/arm/bbb-fpp-reserve-memory.dtbo
	cp /opt/source/bb.org-overlays/src/arm/bbb-fpp-reserve-memory.dtbo .

