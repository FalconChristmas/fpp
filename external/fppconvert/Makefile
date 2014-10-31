
TARGETS = fppconvert

OBJECTS = fppconvert.o NetInfo.o

CPP = g++

DISABLE_WARNINGS = -Wno-reorder -Wno-sign-compare -Wno-unused-variable -Wno-switch -Wno-unused-but-set-variable -Wno-parentheses -Wno-return-type -Wno-uninitialized -Wno-unused-value -Wno-sequence-point

CFLAGS = -g -DFPP -DwxUSE_UNICODE -std=gnu++0x -O2 -std=c++0x -Wall `wx-config --version=3.0 --cflags` $(DISABLE_WARNINGS)

LDFLAGS = -g -s `wx-config --version=3.0 --libs std,media`

all: $(TARGETS)

fppconvert: $(OBJECTS)
	$(CPP) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

xLights/xLights/NetInfo.h:
	git clone https://github.com/smeighan/xLights.git xLights

xLights/xLights/NetInfo.cpp:
	git clone https://github.com/smeighan/xLights.git xLights

NetInfo.o: xLights/xLights/NetInfo.cpp xLights/xLights/NetInfo.h
	$(CPP) $(CFLAGS) -c xLights/xLights/NetInfo.cpp -o $@

fppconvert.o: fppconvert.cpp xLights/xLights/NetInfo.h
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) fppconvert xLights
