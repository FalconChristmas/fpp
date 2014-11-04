
TARGETS = fppconvert

OBJECTS = fppconvert.o NetInfo.o

CPP = g++

DISABLE_WARNINGS = -Wno-reorder -Wno-sign-compare -Wno-unused-variable -Wno-switch -Wno-unused-but-set-variable -Wno-parentheses -Wno-return-type -Wno-uninitialized -Wno-unused-value -Wno-sequence-point -ferror-limit=50000

CFLAGS = -g -DFPP -O2 -std=c++0x -Wall $(DISABLE_WARNINGS)

LDFLAGS = -g 

all: $(TARGETS)

fppconvert: $(OBJECTS)
	$(CPP) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

xLights/xLights/NetInfo.h:
	git clone -b OpenGL https://github.com/smeighan/xLights.git xLights

xLights/xLights/NetInfo.cpp:
	git clone -B OpenGL https://github.com/smeighan/xLights.git xLights

NetInfo.o: xLights/xLights/NetInfo.cpp xLights/xLights/NetInfo.h
	$(CPP) $(CFLAGS) -c xLights/xLights/NetInfo.cpp -o $@

fppconvert.o: fppconvert.cpp xLights/xLights/NetInfo.h
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) fppconvert xLights
