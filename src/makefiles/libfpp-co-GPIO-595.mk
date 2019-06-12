OBJECTS_fpp_co_GPIO_595_so += channeloutput/GPIO595.o
LIBS_fpp_co_GPIO_595_so += -L. -lfpp

TARGETS += libfpp-co-GPIO-595.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_GPIO_595_so)

libfpp-co-GPIO-595.so: libfpp.so $(OBJECTS_fpp_co_GPIO_595_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GPIO_595_so) $(LIBS_fpp_co_GPIO_595_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GPIO_595_so) -o $@

