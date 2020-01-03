
OBJECTS_fpp_co_matrix_ColorLight5a75_so += channeloutput/ColorLight-5a-75.o
LIBS_fpp_co_matrix_ColorLight5a75_so += -L. -lfpp

TARGETS += libfpp-co-matrix-ColorLight5a75.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_ColorLight5a75_so)

libfpp-co-matrix-ColorLight5a75.so: $(OBJECTS_fpp_co_matrix_ColorLight5a75_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(CFLAGS) $(LDFLAGS) $(LIBS_fpp_co_matrix_ColorLight5a75_so) $(LDFLAGS_fpp_co_matrix_ColorLight5a75_so) $(OBJECTS_fpp_co_matrix_ColorLight5a75_so) -o $@

