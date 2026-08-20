CXXCOMPILER := g++
CCOMPILER := gcc
# CC has been set to g++ so keep for compatibility with C++ plugins
CC := g++


#CXXCOMPILER := clang++
#CCOMPILER := clang
# CC has been set to g++ so keep for compatibility with C++ plugins
#CC := clang++

ifneq ($(wildcard /usr/bin/ccache),)
ifeq ($(DISTCC_HOSTS),)
	CCACHE = ccache
else
	CCACHE = ccache distcc
	# distcc keeps its per-slot lock files (mode 0600) under DISTCC_DIR, which
	# defaults to $HOME/.distcc. FPP reaches distcc through a PHP -> sudo chain
	# where HOME is unreliable: the "Update FPP Now" button runs as the apache
	# "fpp" user (HOME=/home/fpp, which is root-owned and NOT writable by fpp),
	# other paths unset HOME or reset it to /root. So the default lands distcc
	# in an unwritable/nonexistent directory and every compile fails to lock,
	# falling back to local-only. Pin a fixed, HOME-independent directory keyed
	# to the uid actually running make -- which IS the distcc client -- so root
	# and fpp never collide on each other's 0600 lock files.
	export DISTCC_DIR := /tmp/.fpp-distcc-$(shell id -u)
	# Assign the mkdir to a throwaway var: a bare $(shell ...) on a tab-indented
	# line is parsed as a recipe ("recipe commences before first target"); an
	# assignment is not.
	DISTCC_DIR_MKDIR := $(shell mkdir -p $(DISTCC_DIR))
endif
endif

# nocc: use it as the compiler launcher DIRECTLY (override the ccache set above,
# if any). Do NOT wrap it as "ccache nocc": ccache would run the preprocessor
# LOCALLY (cc1plus -E) to compute its cache key -- exactly the bottleneck nocc
# exists to remove -- and a NOPCH build never matches the PCH-built global ccache
# anyway. nocc does a light include scan and preprocesses+compiles on the helper,
# so nothing heavy runs on the (often single-core) client.
ifneq ($(NOCC_SERVERS),)
CCACHE = nocc
# nocc also requires the daemon path in NOCC_GO_EXECUTABLE -- it errors out even
# when just running a local link if it is unset. FPP's SetupBuildEnv exports it,
# but default it here (only if unset) so a manual "NOCC_SERVERS=host:port make"
# works too; the nocc deb installs the daemon at /usr/bin/nocc-daemon.
NOCC_GO_EXECUTABLE ?= /usr/bin/nocc-daemon
export NOCC_GO_EXECUTABLE
endif


TARGETS =
SUBMODULES =

ARCH := $(shell cat /etc/fpp/platform 2> /dev/null)
ifeq '$(ARCH)' ''
UNAME := $(shell uname 2> /dev/null)
ifeq '$(UNAME)' 'Darwin'
ARCH = OSX
CXXCOMPILER := clang++
CCOMPILER := clang
CC := clang++
endif
ifeq '$(UNAME)' 'Linux'
ARCH = Linux
endif
endif
ifeq '$(ARCH)' ''
	ARCH = "UNKNOWN"
endif
$(shell echo "Building FPP on '$(ARCH)' platform" 1>&2)

ifeq '$(SRCDIR)' ''
    SRCDIR=/opt/fpp/src
endif

# A "distributed compile" is active when either distcc (DISTCC_HOSTS) or nocc
# (NOCC_SERVERS) is configured. Both disable the PCH and use the target-triplet
# compiler. distcc runs behind ccache ("ccache distcc"); nocc replaces ccache as
# the launcher entirely (CCACHE=nocc, set above) so nothing is preprocessed
# locally to build a ccache key.
DISTRIBUTED_COMPILE :=
ifneq ($(DISTCC_HOSTS),)
DISTRIBUTED_COMPILE := 1
endif
ifneq ($(NOCC_SERVERS),)
DISTRIBUTED_COMPILE := 1
endif

# When building distributed, invoke the target-triplet compiler (e.g.
# arm-linux-gnueabihf-g++) instead of plain "g++". distcc/nocc run whatever
# compiler NAME we hand them on the helper, so the triplet name makes the helper
# select the matching (cross-)compiler -- letting an aarch64 Pi cross-build for a
# 32-bit BeagleBone. Doing it here means a manual build is just
# "DISTCC_HOSTS=host make" (or "NOCC_SERVERS=host:43210 make") with no compiler
# override. Skipped for clang (macOS) and if the triplet compiler is not
# installed.
ifneq ($(DISTRIBUTED_COMPILE),)
ifeq '$(findstring clang,$(CXXCOMPILER))' ''
DISTCC_TRIPLET := $(shell $(CCOMPILER) -dumpmachine 2>/dev/null)
ifneq ($(wildcard /usr/bin/$(DISTCC_TRIPLET)-g++),)
CXXCOMPILER := $(DISTCC_TRIPLET)-g++
CCOMPILER := $(DISTCC_TRIPLET)-gcc
CC := $(DISTCC_TRIPLET)-g++
endif
endif
endif

# Treat any non-clang compiler as g++. This matches plain "g++" as well as
# target-triplet names like "arm-linux-gnueabihf-g++" used for cross-distcc.
# We test for the absence of "clang" rather than the presence of "g++" because
# "clang++" contains the substring "g++" ($(findstring g++,clang++) -> g++).
ifeq '$(findstring clang,$(CXXCOMPILER))' ''
	# Build branch detection -- drives the -g1 release flag below.
	# Resolved in three steps so detached-HEAD checkouts (CI ref builds,
	# tag builds) don't silently drop -g1 and bust the ccache:
	#   1. Explicit FPPBRANCH from the env/CLI wins (set by CI).
	#   2. Local clones on a branch: rev-parse --abbrev-ref.
	#   3. Detached HEAD: probe for a known branch that contains HEAD.
	ifeq ($(FPPBRANCH),)
		FPPBRANCH := $(shell git -C $(SRCDIR) rev-parse --abbrev-ref HEAD 2>/dev/null)
	endif
	ifeq ($(FPPBRANCH),HEAD)
		FPPBRANCH := $(shell git -C $(SRCDIR) for-each-ref --contains HEAD \
			--format='%(refname:short)' \
			refs/heads/master refs/heads/main \
			refs/remotes/origin/master refs/remotes/origin/main \
			2>/dev/null | head -1 | sed 's|^origin/||')
	endif
    # Common CFLAGS
ifeq ($(DISTRIBUTED_COMPILE),)
    PCH_FILE=fpp-pch.h.gch
	# Flags that mean something only to a compile that consumes the PCH.  Objects
	# which deliberately opt out filter this same variable back out rather than
	# repeating its contents, so the two lists cannot drift apart -- see
	# makefiles/fppinit.mk.
	#
	# -Winvalid-pch: a .gch gcc decides it cannot use is discarded SILENTLY, and
	# the build then re-parses the whole of fpp-pch.h in every TU -- slower than
	# having no PCH at all, with nothing in the output to say so. That state went
	# unnoticed for two months (see the note on the fpp-pch.h.gch rule in
	# ../Makefile). This makes the next one loud instead: one warning per TU.
	PCH_CFLAGS := -fpch-preprocess -Winvalid-pch
	CFLAGS+=$(PCH_CFLAGS)
else
	CFLAGS+=-DNOPCH
endif
    OPTIMIZE_FLAGS+=-g1 -O3 -Wno-psabi
    debug: OPTIMIZE_FLAGS=-g -DDEBUG -Wno-psabi
	ifeq '$(FPPDEBUG)' '1'
	    OPTIMIZE_FLAGS=-g -DDEBUG -Wno-psabi
	endif
    asan: OPTIMIZE_FLAGS=-g -O1 -Wno-psabi -fsanitize=address -fno-omit-frame-pointer
    asan: LDFLAGS+=-fsanitize=address
    tsan: OPTIMIZE_FLAGS=-g -O1 -Wno-psabi -fsanitize=thread -fno-omit-frame-pointer
    tsan: LDFLAGS+=-fsanitize=thread
    CXXFLAGS += -std=gnu++23
else
    OPTIMIZE_FLAGS=-O3
    debug: OPTIMIZE_FLAGS=-g -DDEBUG
	ifeq '$(FPPDEBUG)' '1'
	OPTIMIZE_FLAGS=-g -DDEBUG
	endif
    asan: OPTIMIZE_FLAGS=-g -O1 -fsanitize=address -fno-omit-frame-pointer
    asan: LDFLAGS+=-fsanitize=address
    tsan: OPTIMIZE_FLAGS=-g -O1 -fsanitize=thread -fno-omit-frame-pointer
    tsan: LDFLAGS+=-fsanitize=thread
    CXXFLAGS += -std=c++20
endif

# If the mold or gold linker is availabe and we're using g++, we'll
# go ahead and use it as it's MUCH faster (non-clang == g++, see note above)
ifeq '$(findstring clang,$(CXXCOMPILER))' ''
# Give every shared object an $ORIGIN RUNPATH: a dlopen()ed plugin resolves its
# OWN dependencies (libfpp-co-GenericUDP needs libfpp-co-UDPOutput) through its
# own RUNPATH, not the executable's -- without this, such a plugin only loads
# if something else already pulled its dependency into the process.
LDFLAGS += -Wl,-rpath,'$$ORIGIN'
ifneq ($(wildcard /usr/bin/ld.mold),)
# Use a wrapper ld.mold (via -B) that strips a harmless, un-suppressible
# mimalloc over-allocation warning printed by mold's bundled allocator.
MOLD_WRAPPER_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/mold-wrapper
LDFLAGS += -fuse-ld=mold -B$(MOLD_WRAPPER_DIR)
else ifneq ($(wildcard /usr/bin/ld.gold),)
LDFLAGS += -fuse-ld=gold
endif
endif


CFLAGS+=$(OPTIMIZE_FLAGS) -pipe \
	-I $(SRCDIR) \
	-I /usr/include/jsoncpp \
	-fpic

# Auto-generate per-object header dependency files (-MMD) with phony targets
# for removed headers (-MP), so a change to a core header (e.g. Commands.h)
# correctly invalidates any plugin object that transitively includes it -
# without this, `make` only knows about a .cpp's *own* prerequisites and
# silently reports "Nothing to be done" after a core header changes,
# leaving a stale, potentially ABI-incompatible .so in place.
#
# Scoped to plugin builds only (not the core itself): the core's own
# Makefile sets SRCDIR to its own directory, so CURDIR == SRCDIR there;
# every plugin Makefile sets SRCDIR ?= /opt/fpp/src as an *external*
# reference, so CURDIR != SRCDIR. This keeps the core build (which has its
# own, separate staleness story) free of the .d files this generates.
ifneq ($(abspath $(CURDIR)),$(abspath $(SRCDIR)))
CFLAGS += -MMD -MP

# A plugin that declares FPP_PLUGIN_SUPPORTS_UNLOAD gets dlclose()d when it is
# unloaded, and glibc permanently marks any shared object that is the FIRST to
# define an STB_GNU_UNIQUE symbol as NODELETE: dlclose() then returns success
# and unmaps nothing, for the life of the process.
#
# GCC emits STB_GNU_UNIQUE for a static local in an inline function - which
# includes every method a plugin defines inside its class body. So one
# "static const std::string" in a formatting helper is enough to make a whole
# plugin impossible to unmap, with no diagnostic anywhere: the unload still
# reports success and everything else about it works. Measured on fpp-PixelRadio,
# which defines 7 of them (three static locals, their guard variables, and a
# static char lookup table): it never unmapped, through any number of cycles,
# until this flag was added - then it unmapped on the first unload, source
# unchanged. Plugins whose only unique symbol is std::piecewise_construct are
# unaffected, because libstdc++ defines that one first.
#
# Check a plugin with:  nm -D lib<plugin>.so | awk '$$2=="u"'
#
# The trade is the point of the flag: statics in inline functions are no longer
# unified across the plugin and libfpp. That sharing is exactly what glibc
# refuses to unload a library to protect, so it cannot be kept alongside
# unloading - and a plugin is the wrong place to be relying on it.
#
# Not applied to the core build above, which is never dlclose()d. Not applied
# for clang either (macOS): Mach-O has no STB_GNU_UNIQUE and clang rejects the
# flag.
ifeq '$(findstring clang,$(CXXCOMPILER))' ''
CFLAGS += -fno-gnu-unique
endif

# Pull in whatever dependency files already exist in the plugin's own build
# directory. This is intentionally a plain wildcard (not tied to any
# plugin-specific OBJECTS variable name) so it works unmodified for every
# plugin that includes this file. -include (vs include) means a missing .d
# - e.g. before a plugin's very first build - is silently ignored instead
# of erroring.
#
# Gotcha: this file is included at the *top* of every plugin's Makefile,
# before that Makefile defines its own "all" target. A .d file's rules
# (e.g. "src/Foo.o: src/Foo.cpp Commands.h ...") are themselves targets, and
# GNU Make's default goal is the first target seen across every file it
# reads, included files included - so without the line below, a bare `make`
# would silently build the first *.o instead of "all" once any .d file
# exists. Every plugin Makefile here follows the same "all: ..." convention
# (see e.g. fpp-Capture/Makefile), so pin it explicitly rather than let
# inclusion order decide it by accident.
.DEFAULT_GOAL := all

-include $(wildcard *.d)
-include $(wildcard */*.d)
endif

# The core build needs the same header dependency generation, for the same
# reason: its pattern rules list only a .cpp's *own* same-basename header
# ("%.o: %.cpp %.h"), so a change to any other header does not rebuild the
# objects that include it.  That silently links stale objects into libfpp.so
# while reporting success - a cross-file header edit could be built, tested
# and shipped without ever taking effect.
#
# Applied through $(DEPFLAGS) on the individual compile recipes rather than
# CFLAGS, because the core also passes CFLAGS to the link lines that produce
# the libfpp-co-*.so plugins, where -MMD has nothing to generate from and
# would only write stray .d files named after the library.
#
# Left empty when the branch above already added the flags to CFLAGS, so an
# out-of-tree core build - which takes that branch, since it sets SRCDIR to
# ../src and so trips the CURDIR != SRCDIR test - does not get them twice.
#
# Paired with the -include of $(DEPFILES) at the end of Makefile; the flags
# alone only write the .d files, they do not make anything read them.
ifeq ($(filter -MMD,$(CFLAGS)),)
DEPFLAGS := -MMD -MP
else
DEPFLAGS :=
endif
