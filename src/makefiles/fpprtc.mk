ifeq '$(BUILD_FPPRTC)' '1'
OBJECTS_fpprtc += \
	boot/FPPRTC.o common_mini.o

TARGETS += fpprtc
OBJECTS_ALL+=$(OBJECTS_fpprtc)

fpprtc: $(OBJECTS_fpprtc)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

endif

