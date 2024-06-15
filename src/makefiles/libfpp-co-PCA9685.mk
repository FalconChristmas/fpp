OBJECTS_fpp_co_PCA9685_so += channeloutput/PCA9685.o
LIBS_fpp_co_PCA9685_so=-L. -lfpp -ljsoncpp -lfpp_capeutils  -Wl,-rpath=$(SRCDIR):.

TARGETS += libfpp-co-PCA9685.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_PCA9685_so)

libfpp-co-PCA9685.$(SHLIB_EXT): $(OBJECTS_fpp_co_PCA9685_so) libfpp.$(SHLIB_EXT) libfpp_capeutils.$(SHLIB_EXT) 
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_PCA9685_so) $(LIBS_fpp_co_PCA9685_so) $(LDFLAGS) $(LDFLAGS_fpp_co_PCA9685_so) -o $@

