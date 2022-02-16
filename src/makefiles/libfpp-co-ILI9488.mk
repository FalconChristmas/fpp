ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_co_ILI9488_so += channeloutput/ILI9488.o
LIBS_fpp_co_ILI9488_so=-L. -lfpp -ljsoncpp


TARGETS += libfpp-co-ILI9488.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_ILI9488_so)

libfpp-co-ILI9488.$(SHLIB_EXT): $(OBJECTS_fpp_co_ILI9488_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_ILI9488_so) $(LIBS_fpp_co_ILI9488_so) $(LDFLAGS) $(LDFLAGS_fpp_co_ILI9488_so) -o $@

endif
