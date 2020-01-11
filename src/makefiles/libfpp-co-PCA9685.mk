OBJECTS_fpp_co_PCA9685_so += channeloutput/PCA9685.o
LIBS_fpp_co_PCA9685_so=-L. -lfpp

TARGETS += libfpp-co-PCA9685.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_PCA9685_so)

libfpp-co-PCA9685.so: $(OBJECTS_fpp_co_PCA9685_so) libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_PCA9685_so) $(LIBS_fpp_co_PCA9685_so) $(LDFLAGS) $(LDFLAGS_fpp_co_PCA9685_so) -o $@

