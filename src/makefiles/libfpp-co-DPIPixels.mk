ifeq '$(ARCH)' 'Raspberry Pi'

OBJECTS_fpp_co_dpi_pixels_so += channeloutput/DPIPixels.o
LIBS_fpp_co_dpi_pixels_so=-L. -lfpp

CXXFLAGS_channeloutput/DPIPixels.o=

TARGETS += libfpp-co-DPIPixels.so
OBJECTS_ALL+=$(OBJECTS_fpp_co_dpi_pixels_so)

libfpp-co-DPIPixels.so: $(OBJECTS_fpp_co_dpi_pixels_so)  libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_co_dpi_pixels_so) $(LIBS_fpp_co_dpi_pixels_so) $(LDFLAGS) $(LDFLAGS_fpp_co_dpi_pixels_so) -o $@

endif
