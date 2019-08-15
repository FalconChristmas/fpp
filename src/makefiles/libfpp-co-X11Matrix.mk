ifneq ($(wildcard /usr/include/X11/Xlib.h),)

OJBECTS_fpp_co_X11Matrix_so += channeloutput/X11Matrix.o
LIBS_fpp_co_X11Matrix_so=-lX11  -L. -lfpp

TARGETS += libfpp-co-X11Matrix.so
OBJECTS_ALL+=$(OJBECTS_fpp_co_X11Matrix_so)

libfpp-co-X11Matrix.so: $(OJBECTS_fpp_co_X11Matrix_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(OJBECTS_fpp_co_X11Matrix_so) $(LIBS_fpp_co_X11Matrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_X11Matrix_so) -o $@

endif
