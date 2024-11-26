# BeagleBone
ifeq ($(ISBEAGLEBONE), 1)

# The subtype for the BBB matrices is "LEDscapeMatrix" as
# it was based off of the LEDscape code a long long time ago.  That
# has long since changed, but the subType has remained for compatibility


OBJECTS_fpp_co_matrix_LEDscapeMatrix_so += channeloutput/BBBMatrix.o
LIBS_fpp_co_matrix_LEDscapeMatrix_so += -L. -lfpp -ljsoncpp

TARGETS += libfpp-co-matrix-LEDscapeMatrix.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_co_matrix_LEDscapeMatrix_so)

libfpp-co-matrix-LEDscapeMatrix.$(SHLIB_EXT): $(OBJECTS_fpp_co_matrix_LEDscapeMatrix_so) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_matrix_LEDscapeMatrix_so) $(LIBS_fpp_co_matrix_LEDscapeMatrix_so) $(LDFLAGS) $(LDFLAGS_fpp_co_matrix_LEDscapeMatrix_so) -o $@

endif
