CC := g++

BASEDIR := $(shell basename `pwd`)

SRCDIRS := channeloutput channeloutput/processors channeltester fseq mediaoutput oled playlist pru sensors util

SRCDIR = ./
ifneq '$(BASEDIR)' 'src'
	VPATH = ../src/
	SRCDIR = ../src/
	MKDIROUT = $(shell mkdir $(SRCDIRS))
endif

ifneq ($(wildcard /usr/bin/ccache),)
	CCACHE = ccache
endif

TARGETS =
SUBMODULES =

ARCH := $(shell cat /etc/fpp/platform 2> /dev/null)
ifeq '$(ARCH)' ''
UNAME := $(shell uname 2> /dev/null)
ifeq '$(UNAME)' 'Darwin'
ARCH = OSX
endif
endif
ifeq '$(ARCH)' ''
	ARCH = "UNKNOWN"
endif
$(shell echo "Building FPP on '$(ARCH)' platform" 1>&2)

# Common CFLAGS
CFLAGS=-O1 -pipe \
	-I $(SRCDIR) \
	-fpic
CXXFLAGS += -std=gnu++14


# include the platform fragments first....
# Each will have an ifdef for itself
include ${SRCDIR}/makefiles/platform/*.mk

##############################################################################
# Include a local Makefile if one exists to modify any of the above
-include Makefile.local
#
# Examples of some entries that might exist in Makefile.local:
#
#LIBS_fpp_so += -lX11
#CFLAGS += -DUSE_X11
#OBJECTS_fpp_so += channeloutput/X11Matrix.o channeloutput/X11VirtualDisplay.o
##############################################################################

# now include all the targets
include ${SRCDIR}/makefiles/*.mk

PWD := $(shell pwd)

.PHONY: all
all: gitsubmodules $(TARGETS)
.DEFAULT_GOAL := all


.PHONY: gitsubmodules
ifneq '$(SUBMODULES)' ''
gitsubmodules:
	@! ping -q -c 1 github.com > /dev/null 2>&1 || (cd ..; \
		for submodule in $(SUBMODULES); do \
			git submodule sync $$submodule; \
		done; \
		git submodule foreach git fetch; \
		for submodule in $(SUBMODULES); do \
			git submodule update --init $$submodule; \
		done)
else
gitsubmodules:

endif
force:

optimized: all

release: optimized

debug: all
debug: CFLAGS:=-g $(filter-out -O1,$(CFLAGS))

fppversion.c: fppversion.sh force
	@sh $(SRCDIR)fppversion.sh $(PWD)


%.o: %.cpp %.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.c Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CFLAGS_$@) -c $< -o $@

cleanfpp:
	rm -f fppversion.c $(OBJECTS_ALL) $(TARGETS)

clean:
	rm -f fppversion.c $(OBJECTS_ALL) $(TARGETS)
	@if [ -e ../external/RF24/.git ]; then make -C ../external/RF24 clean; fi
	@if [ -e ../external/rpi-rgb-led-matrix/.git ]; then make -C ../external/rpi-rgb-led-matrix clean; fi
	@if [ -e ../external/rpi_ws281x/libws2811.a ]; then rm ../external/rpi_ws281x/*.o ../external/rpi_ws281x/*.a 2> /dev/null; fi

restart: fppd
	sudo ../scripts/fppd_stop
	sudo ../scripts/fppd_start


