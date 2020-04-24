#pragma once

#include <string>

class DisplayDriver {
public:
    DisplayDriver(int lt) : ledType(lt) {}
    virtual ~DisplayDriver() {}
    
    virtual bool initialize(int &i2cBus) = 0;
    
    virtual bool setFont(const std::string &f) { return false; }
    virtual void printString(int x, int y, const std::string &str, bool white = true) = 0;
    virtual void printStringCentered(int y, const std::string &str, bool white = true) = 0;
    virtual void fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white = true) = 0;
    virtual void drawLine(short x0, short y0, short x1, short y1, bool white = true) = 0;
    virtual void drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white = true) = 0;
    virtual void drawRect(short x0, short y0, short x1, short y1, bool white = true) = 0;
    virtual void fillRect(short x0, short y0, short x1, short y1, bool white = true) = 0;
    virtual void clearDisplay() = 0;
    virtual void flushDisplay() = 0;
    virtual void displayOff() = 0;
    
    virtual int getWidth() = 0;
    virtual int getHeight() = 0;
    
    virtual int getMinimumRefresh() { return 1000; }
    
protected:
    int ledType;
};


class SSD1306DisplayDriver : public DisplayDriver {
public:
    SSD1306DisplayDriver(int type);
    virtual ~SSD1306DisplayDriver();
    
    virtual bool initialize(int &i2cBus) override;
    virtual int getWidth() override;
    virtual int getHeight() override;

    virtual bool setFont(const std::string &f);
    virtual void printString(int x, int y, const std::string &str, bool white = true) override;
    virtual void printStringCentered(int y, const std::string &str, bool white = true) override;
    virtual void fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white = true) override;
    virtual void drawLine(short x0, short y0, short x1, short y1, bool white = true) override;
    virtual void drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white = true) override;
    virtual void drawRect(short x0, short y0, short x1, short y1, bool white = true) override;
    virtual void fillRect(short x0, short y0, short x1, short y1, bool white = true) override;
    virtual void clearDisplay() override;
    virtual void flushDisplay() override;
    virtual void displayOff() override;
};



class PinCapabilities;
class I2C1602_2004_DisplayDriver : public DisplayDriver {
public:
    I2C1602_2004_DisplayDriver(int type);
    virtual ~I2C1602_2004_DisplayDriver();
    
    virtual bool initialize(int &i2cBus) override;
    virtual int getWidth() override;
    virtual int getHeight() override;

    virtual bool setFont(const std::string &f);
    virtual void printString(int x, int y, const std::string &str, bool white = true) override;
    virtual void printStringCentered(int y, const std::string &str, bool white = true) override;
    virtual void fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white = true) override;
    virtual void drawLine(short x0, short y0, short x1, short y1, bool white = true) override;
    virtual void drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white = true) override;
    virtual void drawRect(short x0, short y0, short x1, short y1, bool white = true) override;
    virtual void fillRect(short x0, short y0, short x1, short y1, bool white = true) override;
    virtual void clearDisplay() override;
    virtual void flushDisplay() override;
    virtual void displayOff() override;
    virtual int getMinimumRefresh() { return 3000; }

private:
    void write4Bits(uint8_t bits, bool cmd);
    void command(uint8_t bits);
    void send(uint8_t bits, bool cmd);
    std::array<std::string, 4> lines;
    std::array<std::string, 4> lastLines;
};
