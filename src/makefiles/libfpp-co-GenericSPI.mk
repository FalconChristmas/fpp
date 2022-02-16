
OBJECTS_fpp_co_GenericSPI_so += channeloutput/GenericSPI.o
LIBS_fpp_co_GenericSPI_so=-L. -lfpp -ljsoncpp

TARGETS += libfpp-co-GenericSPI.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_GenericSPI_so)

libfpp-co-GenericSPI.$(SHLIB_EXT): $(OBJECTS_fpp_co_GenericSPI_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GenericSPI_so) $(LIBS_fpp_co_GenericSPI_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GenericSPI_so) -o $@

