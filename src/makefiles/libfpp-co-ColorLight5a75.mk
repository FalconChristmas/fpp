
OBJECTS_fpp_co_matrix_ColorLight5a75_so += channeloutput/ColorLight-5a-75.o
LIBS_fpp_co_matrix_ColorLight5a75_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-matrix-ColorLight5a75.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_ColorLight5a75_so)

libfpp-co-matrix-ColorLight5a75.$(SHLIB_EXT): $(OBJECTS_fpp_co_matrix_ColorLight5a75_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(LDFLAGS) $(LIBS_fpp_co_matrix_ColorLight5a75_so) $(LDFLAGS_fpp_co_matrix_ColorLight5a75_so) $(OBJECTS_fpp_co_matrix_ColorLight5a75_so) -o $@

