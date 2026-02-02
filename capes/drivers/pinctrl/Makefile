
%.o: %.cpp 
	ccache g++ -O2 -std=gnu++23 -c $< -o $@

all: pinctrl

pinctrl: src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o src/PocketBeagle1.o src/BeagleBoneBlack.o src/AM335x.o
	g++  -o pinctrl src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o src/PocketBeagle1.o src/BeagleBoneBlack.o src/AM335x.o
	chmod +s pinctrl

install: pinctrl 
	cp -f pinctrl /usr/bin
	chmod +s /usr/bin/pinctrl
clean:
	rm -f pinctrl src/*.o 
