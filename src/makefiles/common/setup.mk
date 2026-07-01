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
# compiler; nocc is injected in front of the compiler via CCACHE_PREFIX (set by
# scripts/functions SetupBuildEnv), so the ccache launcher above stays plain.
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
# "DISTCC_HOSTS=host make" (or "NOCC_SERVERS=host CCACHE_PREFIX=nocc make") with
# no compiler override. Skipped for clang (macOS) and if the triplet compiler is
# not installed.
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
	CFLAGS+=-fpch-preprocess
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

