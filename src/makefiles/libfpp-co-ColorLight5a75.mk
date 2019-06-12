
SRC_fpp_co_matrix_ColorLight5a75_so += channeloutput/ColorLight-5a-75.cpp
LIBS_fpp_co_matrix_ColorLight5a75_so += -L. -lfpp

TARGETS += libfpp-co-matrix-ColorLight5a75.so

libfpp-co-matrix-ColorLight5a75.so: libfpp.so $(OBJECTS_fpp_co_matrix_ColorLight5a75_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) $(SRC_fpp_co_matrix_ColorLight5a75_so) $(LIBS_fpp_co_matrix_ColorLight5a75_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_ColorLight5a75_so) -o $@

