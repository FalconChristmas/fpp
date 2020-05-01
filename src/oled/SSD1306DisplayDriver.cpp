#include "fpp-pch.h"

#include "I2C.h"
#include "SSD1306_OLED.h"
#include "DisplayDriver.h"
#include "RobotoFont-14.h"

extern I2C_DeviceT I2C_DEV_2;
#ifdef PLATFORM_BBB
#include "util/BBBUtils.h"
#define I2C_DEV_PATH I2C_DEV2_PATH
#else
#define I2C_DEV_PATH I2C_DEV1_PATH
#endif


SSD1306DisplayDriver::SSD1306DisplayDriver(int lt) : DisplayDriver(lt) {
    LED_DISPLAY_WIDTH = 128;
    LED_DISPLAY_HEIGHT = 64;
    if (ledType == 3 || ledType == 4) {
        LED_DISPLAY_HEIGHT = 32;
    } else if (ledType == 9 || ledType == 10) {
        LED_DISPLAY_HEIGHT = 128;
        LED_DISPLAY_TYPE = LED_DISPLAY_TYPE_SSD1327;
    }
    if (ledType == 5 || ledType == 6) {
        LED_DISPLAY_TYPE = LED_DISPLAY_TYPE_SH1106;
    }
}
SSD1306DisplayDriver::~SSD1306DisplayDriver() {
    
}
int SSD1306DisplayDriver::getWidth() {
    return LED_DISPLAY_WIDTH;
}
int SSD1306DisplayDriver::getHeight() {
    return LED_DISPLAY_HEIGHT;
}

bool SSD1306DisplayDriver::initialize(int &i2cBus) {
    if (init_i2c_dev2(I2C_DEV_PATH, SSD1306_OLED_ADDR) != 0) {
        printf("(Main)i2c: OOPS! Something Went Wrong\n");
        return false;
    }
    if (ledType && display_Init_seq()) {
#ifdef PLATFORM_BBB
        //was not able to configure the led on I2C2, lets see if
        //it's available on I2C1
        Close_device(I2C_DEV_2.fd_i2c);
        if (getBeagleBoneType() == BeagleBoneType::PocketBeagle) {
            PinCapabilities::getPinByName("P2-09").configPin("i2c");
            PinCapabilities::getPinByName("P2-11").configPin("i2c");
        } else {
            PinCapabilities::getPinByName("P9-17").configPin("i2c");
            PinCapabilities::getPinByName("P9-18").configPin("i2c");
        }

        if (init_i2c_dev2(I2C_DEV1_PATH, SSD1306_OLED_ADDR) != 0) {
            printf("(Main)i2c1: OOPS! Something Went Wrong\n");
            return false;
        }
        if (display_Init_seq() )  {
            printf("Could not initialize display\n");
            return false;
        }
#else
        printf("Could not initialize display\n");
        return false;
#endif
    }
    i2cBus = I2C_DEV_2.i2c_dev_path[strlen(I2C_DEV_2.i2c_dev_path) - 1] - '0';
    setTextSize(1);
    if (ledType == 2 || ledType == 4 || ledType == 6 || ledType == 8 || ledType == 10) {
        ::setRotation(2);
    } else if (ledType) {
        ::setRotation(0);
    }

    return true;
}

void SSD1306DisplayDriver::printString(int x, int y, const std::string &str, bool white) {
    setTextColor(white ? WHITE : BLACK);
    setCursor(x, y);
    print_str(str.c_str());
}
void SSD1306DisplayDriver::printStringCentered(int y, const std::string &str, bool white) {
    setTextColor(white ? WHITE : BLACK);
    int len = getTextWidth(str.c_str());
    len /= 2;
    setCursor(64 - len, y);
    print_str(str.c_str());
}
void SSD1306DisplayDriver::fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white) {
    ::fillTriangle(x0, y0, x1, y1, x2, y2, white ? WHITE : BLACK);
}
void SSD1306DisplayDriver::drawLine(short x0, short y0, short x1, short y1, bool white) {
    ::drawLine(x0, y0, x1, y1, white ? WHITE : BLACK);
}
void SSD1306DisplayDriver::drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white) {
    ::drawBitmap(x,y , bitmap, w, h, white ? WHITE : BLACK);
}
void SSD1306DisplayDriver::drawRect(short x0, short y0, short x1, short y1, bool white) {
    ::drawRect(x0, y0, x1, y1, white ? WHITE : BLACK);
}
void SSD1306DisplayDriver::fillRect(short x0, short y0, short x1, short y1, bool white) {
    ::fillRect(x0, y0, x1, y1, white ? WHITE : BLACK);
}
void SSD1306DisplayDriver::clearDisplay() {
    ::clearDisplay();
}
void SSD1306DisplayDriver::flushDisplay() {
    ::Display();
}
void SSD1306DisplayDriver::displayOff() {
    ::clearDisplay();
    ::Display();
}
bool SSD1306DisplayDriver::setFont(const std::string &f) {
    if (f == "") {
        setTextFont(nullptr);
        return true;
    }
    if (f == "Roboto_Medium_14") {
        setTextFont(&Roboto_Medium_14);
        return true;
    }
    return false;
}

