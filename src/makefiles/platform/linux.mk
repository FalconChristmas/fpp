#various platforms that need "generic linux flags"
ifeq '$(ARCH)' 'Linux'
ISLINUX=1
else ifeq '$(ARCH)' 'Debian'
ISLINUX=1
else ifeq '$(ARCH)' 'Pine64'
ISLINUX=1
else ifeq '$(ARCH)' 'CHIP'
ISLINUX=1
else ifeq '$(ARCH)' 'UNKNOWN'
ISLINUX=1
endif

ifeq '$(ISLINUX)' '1'
# do something Linux-y

CFLAGS += -DPLATFORM_UNKNOWN
LDFLAGS_fppd += -L.

endif
