# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

CFLAGS += \
	-DPLATFORM_BBB \
    -I/usr/local/include

LIBS_GPIO_ADDITIONS=
OBJECTS_GPIO_ADDITIONS+=util/BBBUtils.o

LIBS_fpp_so += -lprussdrv -L/usr/local/lib/
OBJECTS_fpp_so += util/BBBPruUtils.o


endif
