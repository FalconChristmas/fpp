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

ifeq '$(CXXCOMPILER)' 'g++'
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
	ifeq ($(FPPBRANCH),master)
	OPTIMIZE_FLAGS=-g1
	endif
    GCCVERSIONGTEQ12:=$(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 12)
    # Common CFLAGS
ifeq ($(DISTCC_HOSTS),)
    PCH_FILE=fpp-pch.h.gch
	CFLAGS+=-fpch-preprocess
else
	CFLAGS+=-DNOPCH
endif
    OPTIMIZE_FLAGS+=-O3 -Wno-psabi
    debug: OPTIMIZE_FLAGS=-g -DDEBUG -Wno-psabi
	ifeq '$(FPPDEBUG)' '1'
	    OPTIMIZE_FLAGS=-g -DDEBUG -Wno-psabi
	endif
    asan: OPTIMIZE_FLAGS=-g -O1 -Wno-psabi -fsanitize=address -fno-omit-frame-pointer
    asan: LDFLAGS+=-fsanitize=address
    tsan: OPTIMIZE_FLAGS=-g -O1 -Wno-psabi -fsanitize=thread -fno-omit-frame-pointer
    tsan: LDFLAGS+=-fsanitize=thread
    ifeq "$(GCCVERSIONGTEQ12)" "1"
        CXXFLAGS += -std=gnu++23
    else
        CXXFLAGS += -std=gnu++2a
    endif
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
# go ahead and use it as it's MUCH faster
ifeq '$(CXXCOMPILER)' 'g++'
ifneq ($(wildcard /usr/bin/ld.mold),)
LDFLAGS += -fuse-ld=mold
else ifneq ($(wildcard /usr/bin/ld.gold),)
LDFLAGS += -fuse-ld=gold
endif
endif


CFLAGS+=$(OPTIMIZE_FLAGS) -pipe \
	-I $(SRCDIR) \
	-I /usr/include/jsoncpp \
	-fpic

