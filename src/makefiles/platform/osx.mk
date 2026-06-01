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

# libfpp references a handful of symbols provided by the fppd executable (e.g.
# runMainFPPDLoop, the main-loop flag the HTTP API toggles to request shutdown).
# On Linux a shared library binds those at load time; macOS's two-level
# namespace requires them resolved at link time, so allow them to be looked up
# dynamically -- fppd exports them and dyld binds them when libfpp is loaded.
LDFLAGS_fpp_so += -undefined dynamic_lookup

SHLIB_EXT=dylib

ifneq ($(wildcard $(HOMEBREW)/bin/ccache),)
	CCACHE = ccache
endif


endif
