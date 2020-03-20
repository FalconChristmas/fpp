CXXCOMPILER := g++
CCOMPILER := gcc

# CC has been set to g++ so keep for compatibility with C++ plugins
CC := g++

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


GCCVERSIONGTEQ8:=$(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 8)

ifeq '$(SRCDIR)' '' 
    SRCDIR=/opt/fpp/src
endif

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

CFLAGS+=$(OPTIMIZE_FLAGS) -pipe \
	-I $(SRCDIR) \
	-fpic

