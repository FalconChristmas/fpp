# MacOSX
ifeq '$(ARCH)' 'OSX'

CHIP := $(shell uname -p 2> /dev/null)
ifeq '$(CHIP)' 'arm'
HOMEBREW=/opt/homebrew
else
HOMEBREW=/usr/local
endif


CFLAGS += -DPLATFORM_OSX  -I$(HOMEBREW)/include -Wswitch
LDFLAGS += -L.  -L$(HOMEBREW)/lib

MAGICK_INCLUDE_PATH=-I$(HOMEBREW)/include/GraphicsMagick

OBJECTS_fpp_so += MacOSUtils.o
LIBS_fpp_so+=-framework CoreAudio

SHLIB_EXT=dylib

ifneq ($(wildcard $(HOMEBREW)/bin/ccache),)
	CCACHE = ccache
endif


endif
