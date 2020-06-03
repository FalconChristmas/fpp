
BASEURL=https://raw.githubusercontent.com/beagleboard/linux/4.19


obj-m := pcm5102a.o pcm512x-i2c.o pcm512x.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: pcm5102a.c pcm512x-i2c.c pcm512x.c BBB-AUDIO-PCM5012A-00A0.dtbo bbb-fpp-reserve-memory.dtbo
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f pcm5102a.c pcm512x.h pcm512x.c pcm512x-i2c.c BBB-AUDIO-PCM5012A-00A0.dtbo  bbb-fpp-reserve-memory.dtbo

install:
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra/
	sudo cp *.ko /lib/modules/$(shell uname -r)/extra/
	sudo depmod -a
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

pcm5102a.c:
	curl ${BASEURL}/sound/soc/codecs/pcm5102a.c -o pcm5102a.c

pcm512x.c:
	curl ${BASEURL}/sound/soc/codecs/pcm512x.h -o pcm512x.h
	curl ${BASEURL}/sound/soc/codecs/pcm512x.c -o pcm512x.c

pcm512x-i2c.c:
	curl ${BASEURL}/sound/soc/codecs/pcm512x-i2c.c -o pcm512x-i2c.c
