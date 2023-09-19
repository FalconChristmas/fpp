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
    GCCVERSIONGTEQ9:=$(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 9)
    # Common CFLAGS
ifeq ($(DISTCC_HOSTS),)
    PCH_FILE=fpp-pch.h.gch
	CFLAGS+=-fpch-preprocess
else
	CFLAGS+=-DNOPCH
endif
    OPTIMIZE_FLAGS=-O3 -Wno-psabi
    debug: OPTIMIZE_FLAGS=-g -DDEBUG -Wno-psabi
    CXXFLAGS += -std=gnu++2a

    ifeq "$(GCCVERSIONGTEQ9)" "0"
		LD_FLAG_FS=-lstdc++fs
	else
		LD_FLAG_FS=
    endif
else
    OPTIMIZE_FLAGS=-O3
    debug: OPTIMIZE_FLAGS=-g -DDEBUG
    CXXFLAGS += -std=c++20
endif



CFLAGS+=$(OPTIMIZE_FLAGS) -pipe \
	-I $(SRCDIR) \
	-fpic

