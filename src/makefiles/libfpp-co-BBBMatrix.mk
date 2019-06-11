# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

# The subtype for the BBB matrices is "LEDscapeMatrix" as
# it was based off of the LEDscape code a long long time ago.  That
# has long since changed, but the subType has remained for compatibility


OBJECTS_fpp_co_matrix_LEDscapeMatrix_so += channeloutput/BBBMatrix.o
LIBS_fpp_co_matrix_LEDscapeMatrix_so += -Lfpp

TARGETS += libfpp-co-matrix-LEDscapeMatrix.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_LEDscapeMatrix_so)

libfpp-co-matrix-LEDscapeMatrix.so: libfpp.so $(OBJECTS_fpp_co_matrix_LEDscapeMatrix_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_matrix_LEDscapeMatrix_so) $(LIBS_fpp_co_matrix_LEDscapeMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_LEDscapeMatrix_so) -o $@

endif
