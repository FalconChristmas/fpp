
OBJECTS_fppd = fppd.o
LIBS_fppd = $(NULL)

GPIOD_CHIPS = $(shell (which gpiodetect > /dev/null && gpiodetect) | wc -l | xargs)

LDFLAGS_fppd += -rdynamic $(shell curl-config --libs) \
	$(shell GraphicsMagick++-config --ldflags --libs) \
	$(shell GraphicsMagickWand-config --ldflags --libs) \
	$(LIBS_GPIO_EXE_ADDITIONS) \
	$(NULL)

TARGETS += fppd
OBJECTS_ALL+=$(OBJECTS_fppd)

CXXFLAGS_fppd.o+=$(MAGICK_INCLUDE_PATH)

ifneq '$(ARCH)' 'OSX'
LIBS_fpp_so+=-Wl,-rpath=$(SRCDIR):$(SRCDIR)/../external/RF24/:.
endif

fppd.o: fppd.cpp fppd.h $(PCH_FILE) fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -DGPIOD_CHIPS=$(GPIOD_CHIPS) -c $(SRCDIR)$< -o $@

fppd: $(OBJECTS_fppd) libfpp.$(SHLIB_EXT) $(DEPENDENCIES_GPIO_ADDITIONS)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -L . -l fpp $(LIBS_fpp_so) -o $@
