LINUX_DIR = ~/src/pi/linux

obj-m += rtc-pcf2127a.o

all:
	make ARCH=arm CROSS_COMPILE=arm-bcm2708hardfp-linux-gnueabi- -C $(LINUX_DIR) M=$(PWD) modules

clean:
	make -C $(LINUX_DIR) M=$(PWD) clean

reallyclean:
	rm rtc-pcf2127a.c

$(PWD)/rtc-pcf2127a.c:
	wget -nd -O $(PWD)/rtc-pcf2127a.c https://raw.github.com/afterthoughtsoftware/linux/rpi-3.6.y/drivers/rtc/rtc-pcf2127a.c

$(PWD)/rtc-pcf2127a.o: $(PWD)/rtc-pcf2127a.c


