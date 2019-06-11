# MacOSX
# The only thing that really builds on OSX at this point is fsequtils
ifeq '$(ARCH)' 'OSX'

$(OBJECTS_fsequtils): CFLAGS += -DPLATFORM_OSX

endif
