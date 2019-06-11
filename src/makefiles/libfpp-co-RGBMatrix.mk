ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_co_matrix_RGBMatrix_so += channeloutput/RGBMatrix.o
LIBS_fpp_co_matrix_RGBMatrix_so=-L../external/rpi-rgb-led-matrix/lib/ -lrgbmatrix -Lfpp

CXXFLAGS_channeloutput/RGBMatrix.o=-I../external/rpi-rgb-led-matrix/include/

TARGETS += libfpp-co-matrix-RGBMatrix.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_RGBMatrix_so)

libfpp-co-matrix-RGBMatrix.so: libfpp.so $(OBJECTS_fpp_co_matrix_RGBMatrix_so) ../external/rpi-rgb-led-matrix/lib/librgbmatrix.a
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_matrix_RGBMatrix_so) $(LIBS_fpp_co_matrix_RGBMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_RGBMatrix_so) -o $@

endif
