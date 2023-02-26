
OBJECTS_fpp_co_FBMatrix_so += channeloutput/FBMatrix.o
LIBS_fpp_co_FBMatrix_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-FBMatrix.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_FBMatrix_so)

libfpp-co-FBMatrix.$(SHLIB_EXT): $(OBJECTS_fpp_co_FBMatrix_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_FBMatrix_so) $(LIBS_fpp_co_FBMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_FBMatrix_so) -o $@
