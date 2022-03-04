OBJECTS_fpp_co_Renard_so += channeloutput/USBRenard.o
LIBS_fpp_co_Renard_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-Renard.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_Renard_so)

libfpp-co-Renard.$(SHLIB_EXT): $(OBJECTS_fpp_co_Renard_so) $(DEPS_fpp_co_Renard_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_Renard_so) $(LIBS_fpp_co_Renard_so) $(LDFLAGS) $(LDFLAGS_fpp_co_Renard_so) -o $@

