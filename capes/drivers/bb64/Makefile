
%.o: %.cpp 
	ccache g++ -O3 -std=gnu++23 -c $(SRCDIR)$< -o $@

pinctrl: src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o
	g++  -o pinctrl src/AM6232.o src/Ball.o src/PocketBeagle2.o src/ConfigPin.o src/Pin.o
	chmod +s pinctrl

install: pinctrl
	cp -f pinctrl /usr/bin
	chmod +s /usr/bin/pinctrl

clean:
	rm -f pinctrl src/*.o

all: pinctrl