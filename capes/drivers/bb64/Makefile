
%.o: %.cpp 
	ccache g++ -O3 -std=gnu++23 -c $(SRCDIR)$< -o $@

all: pinctrl fpp-reserve-memory.dtb fpp-cape-overlay.dtb

pinctrl: src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o
	g++  -o pinctrl src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o
	chmod +s pinctrl

fpp-reserve-memory.dtb: fpp-reserve-memory.dts
	dtc -O dtb -o fpp-reserve-memory.dtb fpp-reserve-memory.dts

fpp-cape-overlay.dtb: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtb fpp-cape-overlay.dts

install: pinctrl fpp-reserve-memory.dtb fpp-cape-overlay.dtb
	cp -f pinctrl /usr/bin
	chmod +s /usr/bin/pinctrl
	cp fpp-reserve-memory.dtb /boot/firmware/overlays
	cp fpp-cape-overlay.dtb /boot/firmware/overlays

clean:
	rm -f pinctrl src/*.o *.dtb

