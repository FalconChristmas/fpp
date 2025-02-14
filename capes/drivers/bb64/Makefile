
%.o: %.cpp 
	ccache g++ -O3 -std=gnu++23 -c $(SRCDIR)$< -o $@

all: pinctrl fpp-reserve-memory.dtb fpp-cape-overlay.dtb

pinctrl: src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o
	g++  -o pinctrl src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o
	chmod +s pinctrl

fpp-reserve-memory.dtb: fpp-reserve-memory.dts
	cpp -I/opt/source/dtb-6.12-Beagle/include/ -I/opt/source/dtb-6.12-Beagle/src/arm64/ti/ -nostdinc -undef -x assembler-with-cpp fpp-reserve-memory.dts > .fpp-reserve-memory.dts
	dtc --include /opt/source/dtb-6.12-Beagle/include/ --include /opt/source/dtb-6.12-Beagle/src/arm64/ti/ -O dtb -o fpp-reserve-memory.dtb .fpp-reserve-memory.dts	
	rm .fpp-reserve-memory.dts	

fpp-cape-overlay.dtb: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtb fpp-cape-overlay.dts

install_cape_overlay:
	dtc -O dtb -o /boot/firmware/overlays/fpp-cape-overlay.dtb /opt/fpp/capes/drivers/bb64/fpp-cape-overlay.dts

install: pinctrl fpp-reserve-memory.dtb fpp-cape-overlay.dtb
	cp -f pinctrl /usr/bin
	chmod +s /usr/bin/pinctrl
	cp fpp-reserve-memory.dtb /boot/firmware/overlays
	cp fpp-cape-overlay.dtb /boot/firmware/overlays

clean:
	rm -f pinctrl src/*.o *.dtb
