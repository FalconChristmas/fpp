OBJECTS_fpp_co_GPIO_595_so += channeloutput/GPIO595.o
LIBS_fpp_co_GPIO_595_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-GPIO-595.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_GPIO_595_so)

libfpp-co-GPIO-595.$(SHLIB_EXT): $(OBJECTS_fpp_co_GPIO_595_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_GPIO_595_so) $(LIBS_fpp_co_GPIO_595_so) $(LDFLAGS) $(LDFLAGS_fpp_co_GPIO_595_so) -o $@

