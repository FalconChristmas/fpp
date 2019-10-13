
OBJECTS_fpp_co_GenericSPI_so += channeloutput/GenericSPI.o
LIBS_fpp_co_GenericSPI_so=-L. -lfpp

TARGETS += libfpp-co-GenericSPI.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_GenericSPI_so)

libfpp-co-GenericSPI.so: $(OBJECTS_fpp_co_GenericSPI_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GenericSPI_so) $(LIBS_fpp_co_GenericSPI_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GenericSPI_so) -o $@

