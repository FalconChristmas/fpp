
OBJECTS_fppd = fppd.o
LIBS_fppd = $(NULL)


LDFLAGS_fppd += -rdynamic $(shell curl-config --libs) \
	$(shell GraphicsMagick++-config --ldflags --libs) \
	$(shell GraphicsMagickWand-config --ldflags --libs) \
	$(NULL)

TARGETS += fppd
OBJECTS_ALL+=$(OBJECTS_fppd)

fppd: $(OBJECTS_fppd) libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -L . -l fpp $(LIBS_fpp_so) -o $@
	$(ADJUST_PATHS_COMMAND)
