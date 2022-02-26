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
ifneq ($(wildcard /opt/homebrew/bin/ccache),)
	CCACHE = ccache
endif

ifneq ($(wildcard /usr/local/bin/gcc-[0-9]*),)
CXXCOMPILER := $(wildcard /usr/local/bin/g++-[0-9]*)
CCOMPILER := $(wildcard /usr/local/bin/gcc-[0-9]*)
CC := $(wildcard /usr/local/bin/g++-[0-9]*)
CXXFLAGS += -std=c++20
endif
ifneq ($(wildcard /opt/homebrew/bin/gcc-[0-9]*),)
CXXCOMPILER := $(wildcard /opt/homebrew/bin/g++-[0-9]*)
CCOMPILER := $(wildcard /opt/homebrew/bin/gcc-[0-9]*)
CC := $(wildcard /opt/homebrew/bin/g++-[0-9]*)
CXXFLAGS += -std=c++20
CFLAGS += -fpch-preprocess
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
    GCCVERSIONGTEQ9:=$(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 9)
    # Common CFLAGS
    CFLAGS+=-fpch-preprocess
    OPTIMIZE_FLAGS+=-O3 -Wno-psabi
    debug: OPTIMIZE_FLAGS+=-g -DDEBUG -Wno-psabi
    CXXFLAGS += -std=gnu++2a

    ifeq "$(GCCVERSIONGTEQ9)" "0"
		LD_FLAG_FS=-lstdc++fs
	else
		LD_FLAG_FS=
    endif
else
    OPTIMIZE_FLAGS+=-O3
    debug: OPTIMIZE_FLAGS+=-g -DDEBUG
    #CXXFLAGS += -std=gnu++2a
endif



CFLAGS+=$(OPTIMIZE_FLAGS) -pipe \
	-I $(SRCDIR) \
	-fpic

