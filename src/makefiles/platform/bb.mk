# BeagleBone Black
ifeq '$(ARCH)' 'BeagleBone Black'

CFLAGS += \
	-DPLATFORM_BBB

LIBS_GPIO_ADDITIONS=
OBJECTS_GPIO_ADDITIONS+=util/BBBUtils.o

OBJECTS_fpp_so += util/BBBPruUtils.o


endif
