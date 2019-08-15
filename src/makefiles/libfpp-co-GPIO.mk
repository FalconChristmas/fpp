OBJECTS_fpp_co_GPIO_so += channeloutput/GPIO.o
LIBS_fpp_co_GPIO_so += -L. -lfpp

TARGETS += libfpp-co-GPIO.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_GPIO_so)

libfpp-co-GPIO.so: $(OBJECTS_fpp_co_GPIO_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GPIO_so) $(LIBS_fpp_co_GPIO_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GPIO_so) -o $@

