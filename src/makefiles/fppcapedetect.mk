
ifeq '$(BUILD_FPPCAPEDETECT)' '1'

OBJECTS_fppcapedetect = non-gpl/CapeUtils/fppcapedetect.o

OBJECTS_libfpp_capeutils_so = non-gpl/CapeUtils/CapeUtils.o

LIBS_fppcapedetect = \
	-L. -ljsoncpp -lcrypto -lfpp_capeutils

LIBS_libfpp_capeutils_so = \
	-ljsoncpp -lcrypto

TARGETS+=fppcapedetect
TARGETS+=libfpp_capeutils.so

LIBS_fppcapedetect+=-Wl,-rpath=$(SRCDIR):.

OBJECTS_ALL+=$(OBJECTS_fppcapedetect) $(OBJECTS_libfpp_capeutils_so)

libfpp_capeutils.so: $(OBJECTS_libfpp_capeutils_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_libfpp_capeutils_so) $(LIBS_libfpp_capeutils_so) $(LDFLAGS) $(LDFLAGS_libfpp_capeutils_so) -o $@

fppcapedetect: $(OBJECTS_fppcapedetect) libfpp_capeutils.so
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@
	
CapeUtils.o: CapeUtils.cpp CapeUtils.h fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

fppcapedetect.o: fppcapedetect.cpp fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CXXCOMPILER) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $(SRCDIR)$< -o $@

endif
