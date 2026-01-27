
%.o: %.cpp 
	ccache g++ -g -std=gnu++23 -c $< -o $@

all: pinctrl fpp-base-overlay.dtb fpp-cape-overlay.dtb

pinctrl: src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o src/PocketBeagle1.o src/BeagleBoneBlack.o src/AM335x.o
	g++  -o pinctrl src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o src/PocketBeagle1.o src/BeagleBoneBlack.o src/AM335x.o
	chmod +s pinctrl

fpp-base-overlay.dtb: fpp-base-overlay.dts
	cpp -I/opt/source/dtb-6.17.x/include/ -I/opt/source/dtb-6.17.x/src/arm64/ti/ -nostdinc -undef -x assembler-with-cpp fpp-base-overlay.dts > .fpp-base-overlay.dts
	dtc --include /opt/source/dtb-6.17.x/include/ --include /opt/source/dtb-6.17.x/src/arm64/ti/ -O dtb -o fpp-base-overlay.dtb .fpp-base-overlay.dts	
	rm .fpp-base-overlay.dts	

fpp-cape-overlay.dtb: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtb fpp-cape-overlay.dts

install_cape_overlay:
	dtc -O dtb -o /boot/firmware/overlays/fpp-cape-overlay.dtb /opt/fpp/capes/drivers/bb64/fpp-cape-overlay.dts

install: pinctrl fpp-base-overlay.dtb fpp-cape-overlay.dtb
	cp -f pinctrl /usr/bin
	chmod +s /usr/bin/pinctrl
	cp fpp-base-overlay.dtb /boot/firmware/overlays
	cp fpp-cape-overlay.dtb /boot/firmware/overlays
	cp fpp-cape-overlay.dtb /boot/firmware/overlays/fpp-cape-overlay-default.dtb
	cp extlinux/extlinux.conf /boot/firmware/extlinux/extlinux.conf

clean:
	rm -f pinctrl src/*.o *.dtb
