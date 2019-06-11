
OBJECTS_fpp_co_matrix_ColorLight5a75_so += channeloutput/ColorLight-5a-75.o
LIBS_fpp_co_matrix_ColorLight5a75_so += -Lfpp

TARGETS += libfpp-co-matrix-ColorLight5a75.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_ColorLight5a75_so)

libfpp-co-matrix-ColorLight5a75.so: libfpp.so $(OBJECTS_fpp_co_matrix_ColorLight5a75_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_matrix_ColorLight5a75_so) $(LIBS_fpp_co_matrix_ColorLight5a75_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_ColorLight5a75_so) -o $@

