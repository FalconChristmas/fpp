OBJECTS_fpp_co_FBMatrix_so += channeloutput/FBMatrix.o
LIBS_fpp_co_FBMatrix_so += -L. -lfpp

TARGETS += libfpp-co-FBMatrix.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_FBMatrix_so)

libfpp-co-FBMatrix.so: $(OBJECTS_fpp_co_FBMatrix_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_FBMatrix_so) $(LIBS_fpp_co_FBMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_FBMatrix_so) -o $@

