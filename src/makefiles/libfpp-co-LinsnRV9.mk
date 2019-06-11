
OBJECTS_fpp_co_matrix_LinsnRV9_so += channeloutput/Linsn-RV9.o
LIBS_fpp_co_matrix_LinsnRV9_so += -Lfpp

TARGETS += libfpp-co-matrix-LinsnRV9.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_LinsnRV9_so)

libfpp-co-matrix-LinsnRV9.so: libfpp.so $(OBJECTS_fpp_co_matrix_LinsnRV9_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_matrix_LinsnRV9_so) $(LIBS_fpp_co_matrix_LinsnRV9_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_LinsnRV9_so) -o $@

