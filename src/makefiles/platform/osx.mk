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

OBJECTS_GPIO_ADDITIONS+=util/TmpFileGPIO.o

MAGICK_INCLUDE_PATH=-I$(HOMEBREW)/include/GraphicsMagick

CXXFLAGS_overlays/PixelOverlay.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_overlays/PixelOverlayEffects.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_playlist/PlaylistEntryImage.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_channeloutput/VirtualDisplayBase.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_fppd.o+=$(MAGICK_INCLUDE_PATH)
OBJECTS_fpp_so += MacOSUtils.o

LIBS_fpp_so+=-framework CoreAudio

SHLIB_EXT=dylib

ifneq ($(wildcard $(HOMEBREW)/bin/ccache),)
	CCACHE = ccache
endif


endif
