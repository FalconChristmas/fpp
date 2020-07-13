CXXCOMPILER := g++
CCOMPILER := gcc
# CC has been set to g++ so keep for compatibility with C++ plugins
CC := g++


#CXXCOMPILER := clang++
#CCOMPILER := clang
# CC has been set to g++ so keep for compatibility with C++ plugins
#CC := clang++

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
    GCCVERSIONGTEQ8:=$(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 8)
    CFLAGS+=-fpch-preprocess
    # Common CFLAGS
    ifeq "$(GCCVERSIONGTEQ8)" "1"
    OPTIMIZE_FLAGS=-O3 -Wno-psabi
    debug: OPTIMIZE_FLAGS=-g -Wno-psabi
    CXXFLAGS += -std=gnu++2a
    else
    OPTIMIZE_FLAGS=-O1
    debug: OPTIMIZE_FLAGS=-g
    CXXFLAGS += -std=gnu++17
    endif
else
    OPTIMIZE_FLAGS=-O3
    debug: OPTIMIZE_FLAGS=-g
    CXXFLAGS += -std=gnu++2a
endif



CFLAGS+=$(OPTIMIZE_FLAGS) -pipe \
	-I $(SRCDIR) \
	-fpic

