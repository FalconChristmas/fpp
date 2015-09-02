
TARGETS = fppconvert

OBJECTS = fppconvert.o NetInfo.o

# This is a hash from xLights v3.x which is known to work.  Once we work
# through the changes in xLights v4.x, we can update the hash to a known
# working version for use with fppconvert.
CONVERTHASH = 84b686663c69e26cb0522cb8e26c56afbebc09dc

CPP = g++

DISABLE_WARNINGS = -Wno-reorder -Wno-sign-compare -Wno-unused-variable -Wno-switch -Wno-unused-but-set-variable -Wno-parentheses -Wno-return-type -Wno-uninitialized -Wno-unused-value -Wno-sequence-point 

CFLAGS =  -DFPP -O2 -std=c++0x -Wall $(DISABLE_WARNINGS)

LDFLAGS =  

all: $(TARGETS)

fppconvert: $(OBJECTS)
	$(CPP) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

xLights/xLights/NetInfo.h:
	git clone https://github.com/smeighan/xLights.git xLights
	(cd xLights && git checkout $(CONVERTHASH))

xLights/xLights/NetInfo.cpp:
	git clone https://github.com/smeighan/xLights.git xLights
	(cd xLights && git checkout $(CONVERTHASH))

NetInfo.o: xLights/xLights/NetInfo.cpp xLights/xLights/NetInfo.h
	$(CPP) $(CFLAGS) -c xLights/xLights/NetInfo.cpp -o $@

fppconvert.o: fppconvert.cpp xLights/xLights/NetInfo.h
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) fppconvert xLights
