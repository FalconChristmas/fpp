ifneq ($(wildcard /usr/local/include/wiringPi.h),)

OBJECTS_fpp_co_MAX7219Matrix_so += channeloutput/MAX7219Matrix.o
LIBS_fpp_co_MAX7219Matrix_so=-L. -lfpp


TARGETS += libfpp-co-MAX7219Matrix.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_MAX7219Matrix_so)

libfpp-co-MAX7219Matrix.so: libfpp.so $(OBJECTS_fpp_co_MAX7219Matrix_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_MAX7219Matrix_so) $(LIBS_fpp_co_MAX7219Matrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_MAX7219Matrix_so) -o $@

endif
