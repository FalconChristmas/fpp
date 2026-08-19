ifeq '$(BUILD_FPPINIT)' '1'
OBJECTS_fppinit += \
	boot/FPPINIT.o  boot/FPPINIT_Config.o  boot/FPPINIT_Network.o  boot/FPPINIT_Audio.o  common_mini.o

TARGETS := fppinit $(TARGETS)
OBJECTS_ALL+=$(OBJECTS_fppinit)
LIBS_fppinit = -ljsoncpp -lsystemd $(LIBS_GPIO_ADDITIONS)

ifeq '$(BUILD_FPPCAPEDETECT)' '1'
CXXFLAGS_boot/FPPINIT.o+=-DCAPEDETECT=1
CXXFLAGS_boot/FPPINIT_Config.o+=-DCAPEDETECT=1
CXXFLAGS_boot/FPPINIT_Network.o+=-DCAPEDETECT=1
CXXFLAGS_boot/FPPINIT_Audio.o+=-DCAPEDETECT=1
LIBS_fppinit += \
	-L. -lcrypto -Wl,-rpath=$(SRCDIR):.
FPPINIT_DEPS=libfpp_capeutils.so
endif

# These objects do not use the PCH, so drop the PCH-only flags ($(PCH_CFLAGS),
# defined next to where they are added in makefiles/common/setup.mk) along with
# -g1.  Note this is inherited by these targets' prerequisites: see the comment
# on the fpp-pch.h.gch rule in ../Makefile for why that matters.
boot/FPPINIT.o  boot/FPPINIT_Config.o  boot/FPPINIT_Network.o  boot/FPPINIT_Audio.o: CFLAGS := $(filter-out $(PCH_CFLAGS) -g1,$(CFLAGS))


fppinit: $(OBJECTS_fppinit)  $(FPPINIT_DEPS)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

endif

