ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_co_MAX7219Matrix_so += channeloutput/MAX7219Matrix.o
LIBS_fpp_co_MAX7219Matrix_so=-L. -lfpp -ljsoncpp


TARGETS += libfpp-co-MAX7219Matrix.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_MAX7219Matrix_so)

libfpp-co-MAX7219Matrix.$(SHLIB_EXT): $(OBJECTS_fpp_co_MAX7219Matrix_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_MAX7219Matrix_so) $(LIBS_fpp_co_MAX7219Matrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_MAX7219Matrix_so) -o $@

endif
