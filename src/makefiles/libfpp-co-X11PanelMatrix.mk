ifneq ($(wildcard /usr/include/X11/Xlib.h),)

OJBECTS_fpp_co_matrix_X11PanelMatrix_so += channeloutput/X11PanelMatrix.o
LIBS_fpp_co_matrix_X11PanelMatrix_so=-lX11  -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-matrix-X11PanelMatrix.$(SHLIB_EXT)
OBJECTS_ALL+=$(OJBECTS_fpp_co_matrix_X11PanelMatrix_so)

libfpp-co-matrix-X11PanelMatrix.$(SHLIB_EXT): $(OJBECTS_fpp_co_matrix_X11PanelMatrix_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(OJBECTS_fpp_co_matrix_X11PanelMatrix_so) $(LIBS_fpp_co_matrix_X11PanelMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_X11PanelMatrix_so) -o $@

endif
