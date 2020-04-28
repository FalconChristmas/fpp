
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

ifeq '$(CXXCOMPILER)' 'g++'
PCH_FILE=fpp-pch.h.gch
fpp-pch.h.gch: fpp-pch.h common.h settings.h Sequence.h fppversion.h log.h mqtt.h Warnings.h commands/Commands.h
	CCACHE_COMPRESS=true $(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@
	sha1sum $(PCH_FILE) > $(PCH_FILE).sum
endif

%.o: %.cpp %.h $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.cpp $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.c $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CFLAGS_$@) -c $< -o $@

cleanfpp:
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(TARGETS) $(TARGETS_PHP)

clean::
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(TARGETS) $(TARGETS_PHP) $(PCH_FILE) $(PCH_FILE).sum

restart: fppd
	sudo ../scripts/fppd_stop
	sudo ../scripts/fppd_start


