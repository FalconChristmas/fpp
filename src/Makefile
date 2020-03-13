
BASEDIR := $(shell basename `pwd`)
SRCDIRS := channeloutput channeloutput/processors channeltester fseq mediaoutput oled playlist pru sensors util

SRCDIR = ./
ifneq '$(BASEDIR)' 'src'
	VPATH = ../src/
	SRCDIR = ../src/
	MKDIROUT = $(shell mkdir $(SRCDIRS))
endif

include  ${SRCDIR}/makefiles/common/setup.mk

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

TARGETS_PHP := ../www/fppversion.php ../www/fppdefines.php

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

fppversion.c: fppversion.sh
	sh $(SRCDIR)fppversion.sh $(PWD)
	php fppdefines.php > ../www/fppdefines.php
	chmod 664 ../www/fppdefines.php

fppversion_defines.h: fppversion.c

%.o: %.cpp %.h fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.c fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CFLAGS_$@) -c $< -o $@

cleanfpp:
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(TARGETS) $(TARGETS_PHP)

clean::
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(TARGETS) $(TARGETS_PHP)
	@if [ -e ../external/RF24/.git ]; then make -C ../external/RF24 clean; fi
	@if [ -e ../external/rpi-rgb-led-matrix/.git ]; then make -C ../external/rpi-rgb-led-matrix clean; fi
	@if [ -e ../external/rpi_ws281x/libws2811.a ]; then rm ../external/rpi_ws281x/*.o ../external/rpi_ws281x/*.a 2> /dev/null; fi

restart: fppd
	sudo ../scripts/fppd_stop
	sudo ../scripts/fppd_start


