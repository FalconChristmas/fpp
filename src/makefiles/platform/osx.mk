# MacOSX
# The only thing that really builds on OSX at this point is fsequtils
ifeq '$(ARCH)' 'OSX'

CFLAGS += -DPLATFORM_OSX  -I/opt/homebrew/include -Wswitch
LDFLAGS += -L.  -L/opt/homebrew/lib

OBJECTS_GPIO_ADDITIONS+=util/TmpFileGPIO.o

MAGICK_INCLUDE_PATH=-I/opt/homebrew/include/GraphicsMagick


CXXFLAGS_channeloutput/VirtualDisplay.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_overlays/PixelOverlay.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_overlays/PixelOverlayEffects.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_playlist/PlaylistEntryImage.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_fppd.o+=$(MAGICK_INCLUDE_PATH)
OBJECTS_fpp_so += MacOSUtils.o


LIBS_fpp_so+=-framework CoreAudio


SHLIB_EXT=dylib

endif
