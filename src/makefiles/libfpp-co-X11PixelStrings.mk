ifneq ($(wildcard /usr/include/X11/Xlib.h),)

OJBECTS_fpp_co_X11PixelStrings_so += channeloutput/X11PixelStrings.o
LIBS_fpp_co_X11PixelStrings_so=-lX11  -L. -lfpp

TARGETS += libfpp-co-X11PixelStrings.so
OBJECTS_ALL+=$(OJBECTS_fpp_co_X11PixelStrings_so)

libfpp-co-X11PixelStrings.so: $(OJBECTS_fpp_co_X11PixelStrings_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(OJBECTS_fpp_co_X11PixelStrings_so) $(LIBS_fpp_co_X11PixelStrings_so) $(LDFLAGS) $(LDFLAGS_fpp_co_X11PixelStrings_so) -o $@

endif
