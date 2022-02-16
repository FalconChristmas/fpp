ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_co_matrix_RGBMatrix_so += channeloutput/RGBMatrix.o
LIBS_fpp_co_matrix_RGBMatrix_so=-L../external/rpi-rgb-led-matrix/lib/ -lrgbmatrix -L. -lfpp -ljsoncpp

CXXFLAGS_channeloutput/RGBMatrix.o=-I../external/rpi-rgb-led-matrix/include/

TARGETS += libfpp-co-matrix-RGBMatrix.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_RGBMatrix_so)

libfpp-co-matrix-RGBMatrix.$(SHLIB_EXT): $(OBJECTS_fpp_co_matrix_RGBMatrix_so) ../external/rpi-rgb-led-matrix/lib/librgbmatrix.a libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_matrix_RGBMatrix_so) $(LIBS_fpp_co_matrix_RGBMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_RGBMatrix_so) -o $@

endif
