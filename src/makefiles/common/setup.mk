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


GCCVERSIONGTEQ4:=$(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 8)

# Common CFLAGS
ifeq "$(GCCVERSIONGTEQ4)" "1"
CFLAGS=-O3 -Wno-psabi
else
CFLAGS=-O1
endif
CFLAGS+=-pipe \
	-I $(SRCDIR) \
	-fpic
CXXFLAGS += -std=gnu++14

