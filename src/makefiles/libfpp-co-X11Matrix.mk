ifneq ($(wildcard /usr/include/X11/Xlib.h),)

SRC_fpp_co_X11Matrix_so += channeloutput/X11Matrix.cpp
LIBS_fpp_co_X11Matrix_so=-lX11  -L. -lfpp

TARGETS += libfpp-co-X11Matrix.so

libfpp-co-X11Matrix.so: libfpp.so $(SRC_fpp_co_X11Matrix_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) $(SRC_fpp_co_X11Matrix_so) $(LIBS_fpp_co_X11Matrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_X11Matrix_so) -o $@

endif
