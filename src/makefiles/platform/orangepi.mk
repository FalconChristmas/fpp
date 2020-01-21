ifeq '$(ARCH)' 'OrangePi'
# do something for Orange Pi

CFLAGS += \
	-DPLATFORM_ORANGEPI \
	$(NULL)
endif
