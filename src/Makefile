
FULLBASEDIR := $(shell echo `pwd`)
BASEDIR := $(shell basename `pwd`)
SRCDIRS := channeloutput channeloutput/processors channeltester fseq mediaoutput oled playlist pru sensors util

SRCDIR = /opt/fpp/src/
ifneq '$(BASEDIR)' 'src'
	VPATH = ../src/
	SRCDIR = ../src/
	MKDIROUT = $(shell mkdir $(SRCDIRS))
else
	ifneq '$(FULLBASEDIR)' '/opt/fpp/src'
		SRCDIR = $(FULLBASEDIR)/
	endif
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
	@! curl -S -s -o /dev/null -m 2 https://github.com/FalconChristmas/fpp/blob/master/README.md 2>&1 || (cd ..; \
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
fpp-pch.h.gch: fpp-pch.h common.h settings.h Sequence.h fppversion.h log.h mqtt.h Warnings.h commands/Commands.h
	CCACHE_NOHARDLINK=true CCACHE_COMPRESS=true $(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@
	shasum $(PCH_FILE) > $(PCH_FILE).sum
endif

%.o: %.cpp %.h $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

%.o: %.cpp $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

%.o: %.c $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CCOMPILER) $(CFLAGS) $(CFLAGS_$@) -c $(SRCDIR)$< -o $@


rebuildplugins: cleanplugins plugins

plugins:
	@for DIR in /home/fpp/media/plugins/* ; do \
		if [ -d "$${DIR}" -a -f "$${DIR}/Makefile" ] ; then \
			echo "Building plugin $${DIR}" ; \
			make -C "$${DIR}" -f "$${DIR}/Makefile" ; \
		fi ; \
	done

cleanplugins:
	@for DIR in /home/fpp/media/plugins/* ; do \
		if [ -d "$${DIR}" -a -f "$${DIR}/Makefile" ] ; then \
			echo "Cleaning plugin $${DIR}" ; \
			make -C "$${DIR}" -f "$${DIR}/Makefile" clean ; \
		fi ; \
	done

cleanfpp:
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(TARGETS) $(TARGETS_PHP)

clean::
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(TARGETS) $(TARGETS_PHP) $(PCH_FILE) $(PCH_FILE).sum

restart: fppd
	sudo ../scripts/fppd_stop
	sudo ../scripts/fppd_start


