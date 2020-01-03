# MacOSX
# The only thing that really builds on OSX at this point is fsequtils
ifeq '$(ARCH)' 'OSX'

CFLAGS += -DPLATFORM_OSX
LDFLAGS += -L.

endif
