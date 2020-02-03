OBJECTS_fppoled += \
	common.o \
	log.o \
	settings.o \
	fppversion.o \
	oled/I2C.o \
	oled/SSD1306_OLED.o \
	oled/FPPOLEDUtils.o \
	oled/OLEDPages.o \
	oled/FPPStatusOLEDPage.o \
	oled/FPPMainMenu.o \
    oled/fppoled.o \
    util/GPIOUtils.o \
    $(OBJECTS_GPIO_ADDITIONS)

LIBS_fppoled = \
	-ljsoncpp \
	-lpthread -lrt \
	-lcurl \
	-lgpiod \
	-lgpiodcxx \
	$(LIBS_GPIO_ADDITIONS)

TARGETS += fppoled
OBJECTS_ALL+=$(OBJECTS_fppoled)

fppoled: $(OBJECTS_fppoled)
	$(CCACHE) $(CC) $(CFLAGS_$@) $(OBJECTS_$@) $(LIBS_$@) $(LDFLAGS) $(LDFLAGS_$@) -o $@


