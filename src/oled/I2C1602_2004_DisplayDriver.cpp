#include "fpp-pch.h"

#include <unistd.h>

#include "util/GPIOUtils.h"
#include "DisplayDriver.h"

static constexpr int LED_TYPE_0x20_1602 = 11;
static constexpr int LED_TYPE_0x3f_1602 = 12;
static constexpr int LED_TYPE_0x3f_2004 = 13;


static constexpr int RS_PIN = 0;
static constexpr int RW_PIN = 1;
static constexpr int E_PIN = 2;
static constexpr int D4_PIN = 3;
static constexpr int D5_PIN = 4;
static constexpr int D6_PIN = 5;
static constexpr int D7_PIN = 6;

static std::vector<unsigned int> lineOffsets0x3f = {0, 1, 2, 4, 5, 6, 7};
static std::vector<unsigned int> lineOffset0x3fBacklight = { 3 };
static std::vector<unsigned int> lineOffsets0x20 = {15, 14, 13, 12, 11, 10, 9};
static std::vector<unsigned int> lineOffset0x20Backlight = {7, 6, 8};


#ifdef PLATFORM_BBB
static constexpr int DEFAULT_I2C_BUS = 2;
#else
static constexpr int DEFAULT_I2C_BUS = 1;
#endif

#if __has_include(<gpiod.hpp>)
#include <gpiod.hpp>
#endif

I2C1602_2004_DisplayDriver::I2C1602_2004_DisplayDriver(int lt) : DisplayDriver(lt) {
}
I2C1602_2004_DisplayDriver::~I2C1602_2004_DisplayDriver() {
}
int I2C1602_2004_DisplayDriver::getWidth() {
    if (ledType == LED_TYPE_0x20_1602 || ledType == LED_TYPE_0x3f_1602) {
        return 16*6;
    }
    return 20*6;
}
int I2C1602_2004_DisplayDriver::getHeight() {
    if (ledType == LED_TYPE_0x20_1602 || ledType == LED_TYPE_0x3f_1602) {
        return 16;
    }
    return 32;
}

static std::string exec(const std::string &cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
static bool HasI2CDevice(int i, int i2cBus) {
    char buf[256];
    sprintf(buf, "i2cdetect -y -r %d 0x%X 0x%X", i2cBus, i, i);
    std::string result = exec(buf);
    return result.find("--") == std::string::npos;
}
static int findI2CDeviceBus(int i) {
    if (HasI2CDevice(i, DEFAULT_I2C_BUS)) {
        return DEFAULT_I2C_BUS;
    }
    if (DEFAULT_I2C_BUS == 2) {
        if (HasI2CDevice(i, 1)) {
            return 1;
        }
    }
    return -1;
}


// commands
static constexpr int  LCD_CLEARDISPLAY      = 0x01;
static constexpr int  LCD_RETURNHOME        = 0x02;
static constexpr int  LCD_ENTRYMODESET      = 0x04;
static constexpr int  LCD_DISPLAYCONTROL    = 0x08;
static constexpr int  LCD_CURSORSHIFT       = 0x10;
static constexpr int  LCD_FUNCTIONSET       = 0x20;
static constexpr int  LCD_SETCGRAMADDR      = 0x40;
static constexpr int  LCD_SETDDRAMADDR      = 0x80;

// Flags for display on/off control
static constexpr int LCD_DISPLAYON           = 0x04;
static constexpr int LCD_DISPLAYOFF          = 0x00;
static constexpr int LCD_CURSORON            = 0x02;
static constexpr int LCD_CURSOROFF           = 0x00;
static constexpr int LCD_BLINKON             = 0x01;
static constexpr int LCD_BLINKOFF            = 0x00;

// Flags for display entry mode
static constexpr int LCD_ENTRYRIGHT          = 0x00;
static constexpr int LCD_ENTRYLEFT           = 0x02;
static constexpr int LCD_ENTRYSHIFTINCREMENT = 0x01;
static constexpr int LCD_ENTRYSHIFTDECREMENT = 0x00;

// Flags for display/cursor shift
static constexpr int LCD_DISPLAYMOVE = 0x08;
static constexpr int LCD_CURSORMOVE  = 0x00;
static constexpr int LCD_MOVERIGHT   = 0x04;
static constexpr int LCD_MOVELEFT    = 0x00;



static gpiod::chip CHIP;
static gpiod::line_bulk BACKLIGHT;
static gpiod::line_bulk LINES;
static std::vector<int> VALUES = {0, 0, 0, 0, 0, 0, 0};
static std::vector<int> BACKLIGHTVALUES;
static int BACKLIGHTONVALUE = 1;
static int displayshift = 0;
static int displaymode = 0;
static int displaycontrol = 0;


bool I2C1602_2004_DisplayDriver::initialize(int &i2cBus) {
    std::vector<unsigned int> lineOffsets = lineOffsets0x3f;
    i2cBus = DEFAULT_I2C_BUS;
    int device = 0x3f;
    std::vector<unsigned int> blOffset = lineOffset0x3fBacklight;
    std::string deviceType = "pcf8574";
    if (ledType == LED_TYPE_0x20_1602) {
        device = 0x20;
        //deviceType = "pca9675"; // maybe?
        deviceType = "mcp23017";
        lineOffsets = lineOffsets0x20;
        blOffset = lineOffset0x20Backlight;
        BACKLIGHTONVALUE = 0;
    }
    i2cBus = findI2CDeviceBus(device);
    if (i2cBus == -1) {
        return false;
    }
    char buf[128];
    sprintf(buf, "/sys/bus/i2c/devices/i2c-%d/new_device", i2cBus);
    int f = open(buf, O_WRONLY);
    sprintf(buf, "%s 0x%02X", deviceType.c_str(), device);
    write(f, buf, strlen(buf));
    close(f);
    sleep(1);
    
    CHIP.open(deviceType);
    LINES = CHIP.get_lines(lineOffsets);
    LINES.request({
        "FPPOLED",
        ::gpiod::line_request::DIRECTION_OUTPUT,
        0}, VALUES);
    
    BACKLIGHTVALUES.resize(blOffset.size());
    for (auto &a : BACKLIGHTVALUES) {
        a = BACKLIGHTONVALUE ? 0 : 1;
    }
    BACKLIGHT = CHIP.get_lines(blOffset);
    BACKLIGHT.request({
        "FPPOLED",
        ::gpiod::line_request::DIRECTION_OUTPUT,
        0}, BACKLIGHTVALUES);
    

    displayshift   = (LCD_CURSORMOVE | LCD_MOVERIGHT);
    displaymode    = (LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
    displaycontrol = (LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
    command(0x33);
    command(0x32);  //4bit
    command(0x28);  //function set
    
    command(LCD_DISPLAYCONTROL | displaycontrol);
    command(LCD_CURSORSHIFT    | displayshift);
    command(LCD_CLEARDISPLAY);
    std::this_thread::sleep_for(std::chrono::microseconds(1000)); //takes a long time

    command(LCD_ENTRYMODESET   | displaymode);
    
    for (auto & a : BACKLIGHTVALUES) {
        a = BACKLIGHTONVALUE;
    }
    BACKLIGHT.set_values(BACKLIGHTVALUES);
    LINES.set_values(VALUES);
    return true;
}

void I2C1602_2004_DisplayDriver::command(uint8_t bits) {
    send(bits, true);
}
void I2C1602_2004_DisplayDriver::send(uint8_t bits, bool cmd) {
    uint8_t highnib = (bits & 0xf0) >> 4;
    uint8_t lownib = bits & 0xf;
    write4Bits(highnib, cmd);
    write4Bits(lownib, cmd);
}

static void printValues() {
    
    int val = 0;
    for (int x = 0; x < 8; x++) {
        if (VALUES[RS_PIN])     val |= 0x80;
        if (VALUES[E_PIN])      val |= 0x20;
        if (VALUES[RW_PIN])     val |= 0x1;
        if (VALUES[D7_PIN])     val |= 0x2;
        if (VALUES[D6_PIN])     val |= 0x4;
        if (VALUES[D5_PIN])     val |= 0x8;
        if (VALUES[D4_PIN])     val |= 0x10;
    }
    for(int i=8; i; i--) {
        putchar('0'+((val>>(i-1))&1));
    }
    putchar(' ');
}

void I2C1602_2004_DisplayDriver::write4Bits(uint8_t bits, bool cmd) {
    VALUES[RS_PIN] = cmd ? 0 : 1;
    VALUES[D4_PIN] = (bits & 0x1) ? 1 : 0;
    VALUES[D5_PIN] = (bits & 0x2) ? 1 : 0;
    VALUES[D6_PIN] = (bits & 0x4) ? 1 : 0;
    VALUES[D7_PIN] = (bits & 0x8) ? 1 : 0;
    VALUES[E_PIN] = 1;
    LINES.set_values(VALUES);
    std::this_thread::sleep_for(std::chrono::microseconds(50)); // need > 37us to settle
    VALUES[E_PIN] = 0;
    LINES.set_values(VALUES);
    std::this_thread::sleep_for(std::chrono::nanoseconds(500)); // enable pulse must be >450ns
    //putchar('\n');
}



void I2C1602_2004_DisplayDriver::printString(int x, int y, const std::string &str2, bool white) {
    int width = getWidth() / 6;
    int row = y / 8;
    int col = x / 6;
    std::string str = str2;
    if (!white) {
        str = "<" + str + ">";
    }
    for (int i = 0; i < str.length(); i++) {
        if ((col + i) < width) {
            lines[row][col + i] = str[i];
        }
    }
}
void I2C1602_2004_DisplayDriver::printStringCentered(int y, const std::string &str2, bool white) {
    int width = getWidth() / 6;
    std::string str = str2;
    if (!white) {
        str = "<" + str + ">";
    }
    int row = y / 8;
    int col = (width - str.length()) / 2;
    for (int i = 0; i < str.length(); i++) {
        if ((col + i) < width) {
            lines[row][col + i] = str[i];
        }
    }
}
void I2C1602_2004_DisplayDriver::fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white) {
}
void I2C1602_2004_DisplayDriver::drawLine(short x0, short y0, short x1, short y1, bool white) {
}
void I2C1602_2004_DisplayDriver::drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white) {
}
void I2C1602_2004_DisplayDriver::drawRect(short x0, short y0, short x1, short y1, bool white) {
}
void I2C1602_2004_DisplayDriver::fillRect(short x0, short y0, short x1, short y1, bool white) {
}
void I2C1602_2004_DisplayDriver::clearDisplay() {
    int width = getWidth() / 6;
    std::string blank;
    blank.append(width, ' ');
    for (auto &a : lines) {
        a = blank;
    }
    if (!(displaycontrol & LCD_DISPLAYON)) {
        for (auto & a : BACKLIGHTVALUES) {
            a = BACKLIGHTONVALUE;
        }
        BACKLIGHT.set_values(BACKLIGHTVALUES);
        displaycontrol |= LCD_DISPLAYON;
        command(LCD_DISPLAYCONTROL | displaycontrol);
    }
}
void I2C1602_2004_DisplayDriver::displayOff() {
    if (displaycontrol & LCD_DISPLAYON) {
        for (auto & a : BACKLIGHTVALUES) {
            a = BACKLIGHTONVALUE ? 0 : 1;
        }
        BACKLIGHT.set_values(BACKLIGHTVALUES);
        displaycontrol &= ~LCD_DISPLAYON;
        command(LCD_DISPLAYCONTROL | displaycontrol);
    }
}
void I2C1602_2004_DisplayDriver::flushDisplay() {
    int row_offsets[] = { 0x80, 0xC0, 0x94, 0xD4 };
    int width = getWidth() / 6;
    int numRows = 4;
    if (ledType != LED_TYPE_0x3f_2004) {
        numRows = 2;
    }
    for (int row = 0; row < 4; row++) {
        if (width > lines[row].length()) {
            lines[row].append(width - lines[row].length(), ' ');
        }
        if (lines[row].size() > width) {
            lines[row] = lines[row].substr(0, width);
        }
        if (lines[row] != lastLines[row]) {
            command(row_offsets[row]);
            for (int p = 0; p < lines[row].length(); p++) {
                uint8_t t = lines[row][p];
                send(lines[row][p], false);
            }
            lastLines[row] = lines[row];
        }
    }
}

bool I2C1602_2004_DisplayDriver::setFont(const std::string &f) {
    return false;
}

