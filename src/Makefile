
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

# Header dependency files written by $(DEPFLAGS) (see makefiles/common/setup.mk).
# -MMD puts each one next to its object, so this has to be expanded after every
# fragment above has finished appending to OBJECTS_ALL.  Derived from the object
# list rather than a */*.d wildcard because the core nests three deep
# (channeloutput/processors, overlays/wled, non-gpl/BBShiftPanel).
DEPFILES := $(OBJECTS_ALL:.o=.d)

PWD := $(shell pwd)

# Delete a target whose recipe failed. GNU Make keeps it by default, which on a
# memory-tight board turns one OOM-killed compile into a permanently wrong tree:
# when the OOM killer takes cc1plus partway through writing fpp-pch.h.gch, the
# truncated .gch is left behind with a fresh mtime, so make reports it "up to
# date" and never rebuilds it. GCC then silently ignores the invalid PCH (no
# error, not even with -Winvalid-pch) and re-parses the header in every single
# TU -- the build still succeeds, just slower, forever, with nothing to say why.
.DELETE_ON_ERROR:

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

asan: all

tsan: all

local: $(TARGETS)

fppversion.c: fppversion.sh
	sh $(SRCDIR)fppversion.sh $(PWD)
	php fppdefines.php > ../www/fppdefines.php
	chmod 664 ../www/fppdefines.php

fppversion_defines.h: fppversion.c

# non-clang == g++ (incl. target-triplet g++ names); see note in setup.mk
ifeq '$(findstring clang,$(CXXCOMPILER))' ''
# Target-specific variables are inherited by a target's prerequisites, and
# $(PCH_FILE) is a prerequisite of every %.o -- including the fppinit objects,
# which do "CFLAGS := $(filter-out $(PCH_CFLAGS) -g1,$(CFLAGS))".  fppinit is
# prepended to TARGETS, so it is built first and its objects are always what
# pulls the shared .gch in, which means the .gch ends up built with flags that
# no translation unit ever uses.  gcc then silently discards it for every TU
# ("created with 'none' debug info, but used with 'dwarf-2'", or a _REENTRANT
# mismatch where -pthread is also dropped -- visible only under -Winvalid-pch),
# so the whole tree compiles as though there were no PCH while still paying to
# build one.  Pin the .gch's own flags so no dependent's target-specific CFLAGS
# can reach it.
fpp-pch.h.gch: CFLAGS := $(CFLAGS)
fpp-pch.h.gch: fpp-pch.h common.h settings.h Sequence.h fppversion.h log.h mqtt.h Warnings.h commands/Commands.h
	CCACHE_NOHARDLINK=true CCACHE_COMPRESS=true $(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@
	shasum $(PCH_FILE) > $(PCH_FILE).sum
endif

%.o: %.cpp %.h $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(DEPFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

%.o: %.cpp $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(DEPFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

%.o: %.c $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CCOMPILER) $(CFLAGS) $(DEPFLAGS) $(CFLAGS_$@) -c $(SRCDIR)$< -o $@


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
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(DEPFILES) $(TARGETS) $(TARGETS_PHP)

# fpp-pch.h.gch is named literally rather than as $(PCH_FILE): when the PCH is
# disabled, PCH_FILE is empty, and a .gch left behind by an
# earlier PCH-enabled build in the same tree would survive every clean.  However
# leave the PCH_FILE versions there so if a different filename contruct is used
# in the future, it may be cleaned
clean::
	rm -f fppversion.c fppversion_defines.h $(OBJECTS_ALL) $(DEPFILES) $(TARGETS) $(TARGETS_PHP) $(PCH_FILE) $(PCH_FILE).sum fpp-pch.h.gch fpp-pch.h.gch.sum

restart: fppd
	sudo ../scripts/fppd_stop
	sudo ../scripts/fppd_start

# Read back the header dependencies recorded by the last build.  Kept last so
# the rules these files carry can never win GNU Make's "first target seen
# becomes the default goal" race - .DEFAULT_GOAL is pinned above as well.
#
# Wrapped in $(wildcard ...) rather than left to -include: a bare -include of a
# name that does not exist yet sends Make looking for a rule to build it, and
# on a clean tree that is ~200 fruitless implicit-rule searches.  The wildcard
# drops the missing ones up front, and -include keeps a build working if one is
# deleted by hand.
-include $(wildcard $(DEPFILES))
