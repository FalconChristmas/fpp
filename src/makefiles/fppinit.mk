ifeq '$(BUILD_FPPINIT)' '1'
OBJECTS_fppinit += \
	boot/FPPINIT.o  common_mini.o

TARGETS += fppinit
OBJECTS_ALL+=$(OBJECTS_fppinit)

ifeq '$(BUILD_FPPCAPEDETECT)' '1'
CXXFLAGS_boot/FPPINIT.o+=-DCAPEDETECT=1
LIBS_fppinit = \
	-L. -ljsoncpp -lcrypto -Wl,-rpath=$(SRCDIR):.
FPPINIT_DEPS=libfpp_capeutils.so
endif


fppinit: $(OBJECTS_fppinit)  $(FPPINIT_DEPS)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

endif

