ifeq '$(BUILD_FPPOLED)' '1'
OBJECTS_fppoled += \
	common.o \
	common_mini.o \
	log.o \
	settings.o \
	fppversion.o \
	oled/I2C.o \
	oled/SSD1306_OLED.o \
	oled/FPPOLEDUtils.o \
	oled/OLEDPages.o \
	oled/FPPStatusOLEDPage.o \
	oled/NetworkOLEDPage.o \
	oled/FPPMainMenu.o \
    oled/fppoled.o \
    oled/SSD1306DisplayDriver.o \
    oled/I2C1602_2004_DisplayDriver.o \
    util/GPIOUtils.o \
	util/TmpFileGPIO.o \
    $(OBJECTS_GPIO_ADDITIONS)

LIBS_fppoled = \
	-ljsoncpp \
	-lcurl \
	$(LIBS_GPIO_ADDITIONS) -L. $(LIBS_GPIO_EXE_ADDITIONS)

TARGETS += fppoled
OBJECTS_ALL+=$(OBJECTS_fppoled)

ifneq '$(ARCH)' 'OSX'
LIBS_fppoled+=-Wl,-rpath=$(SRCDIR):.
endif

fppoled: $(OBJECTS_fppoled) $(DEPENDENCIES_GPIO_ADDITIONS)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@

endif

