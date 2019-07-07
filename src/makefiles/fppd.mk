
OBJECTS_fppd = fppd.o
LIBS_fppd = $(NULL)


LDFLAGS_fppd += -rdynamic $(shell curl-config --libs) \
	$(shell GraphicsMagick++-config --ldflags --libs) \
	$(shell GraphicsMagickWand-config --ldflags --libs) \
	$(NULL)

TARGETS += fppd
OBJECTS_ALL+=$(OBJECTS_fppd)

fppd: libfpp.so $(OBJECTS_fppd)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -L . -l fpp -Wl,-rpath=$(PWD):$(PWD)/../external/RF24/ $(LIBS_fpp_so) -o $@
