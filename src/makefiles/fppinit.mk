ifeq '$(BUILD_FPPINIT)' '1'
OBJECTS_fppinit += \
	boot/FPPINIT.o  boot/FPPINIT_Config.o  boot/FPPINIT_Network.o  boot/FPPINIT_Audio.o  common_mini.o

TARGETS += fppinit
OBJECTS_ALL+=$(OBJECTS_fppinit)
LIBS_fppinit = -ljsoncpp -lsystemd

ifeq '$(BUILD_FPPCAPEDETECT)' '1'
CXXFLAGS_boot/FPPINIT.o+=-DCAPEDETECT=1
CXXFLAGS_boot/FPPINIT_Config.o+=-DCAPEDETECT=1
CXXFLAGS_boot/FPPINIT_Network.o+=-DCAPEDETECT=1
CXXFLAGS_boot/FPPINIT_Audio.o+=-DCAPEDETECT=1
LIBS_fppinit += \
	-L. -lcrypto -Wl,-rpath=$(SRCDIR):.
FPPINIT_DEPS=libfpp_capeutils.so
endif

boot/FPPINIT.o  boot/FPPINIT_Config.o  boot/FPPINIT_Network.o  boot/FPPINIT_Audio.o: CFLAGS := $(filter-out -fpch-preprocess -g1,$(CFLAGS))


fppinit: $(OBJECTS_fppinit)  $(FPPINIT_DEPS)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

endif

