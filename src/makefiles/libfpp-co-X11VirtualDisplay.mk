ifneq ($(wildcard /usr/include/X11/Xlib.h),)

SRC_fpp_co_X11VirtualDisplay_so += channeloutput/X11VirtualDisplay.cpp
LIBS_fpp_co_X11VirtualDisplay_so=-lX11  -L. -lfpp

TARGETS += libfpp-co-X11VirtualDisplay.so

libfpp-co-X11VirtualDisplay.so: libfpp.so $(SRC_fpp_co_X11VirtualDisplay_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) $(SRC_fpp_co_X11VirtualDisplay_so) $(LIBS_fpp_co_X11VirtualDisplay_so) $(LDFLAGS) $(LDFLAGS_fpp_co_X11VirtualDisplay_so) -o $@

endif
