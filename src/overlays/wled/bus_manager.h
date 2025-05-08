#pragma once

class BusConfig {
public:
    BusConfig(...) {}

    size_t memUsage(size_t digitalCount) const {
        return 0;
    }
    uint8_t type = TYPE_DIGITAL_MIN;
    uint8_t count = 1;
};
class Bus {
public:
    int getStart() const { return 0; }
    int getType() const { return 0; }
    bool hasWhite() const { return false; }
    bool hasRGB() const { return true; }

    int getLength() const;
    uint32_t getPixelColor(int i) const;
    void setPixelColor(int i, uint32_t c);

    void setBrightness(uint32_t b) {}
    void begin() {}

    bool hasCCT() const { return false; }
    bool isOk() const { return true; }
    uint8_t getAutoWhiteMode() const { return 0; }
    static uint8_t getGlobalAWMode() { return 0; }
    bool isOffRefreshRequired() { return false; }
    bool isVirtual() const { return false; }
    bool isPWM() const { return false; }
    bool is2Pin() const { return false; }
    static constexpr unsigned getNumberOfPins(uint8_t type) { return 4; }

    static constexpr bool isTypeValid(uint8_t type) { return (type > 15 && type < 128); }
    static constexpr bool isDigital(uint8_t type) { return (type >= TYPE_DIGITAL_MIN && type <= TYPE_DIGITAL_MAX) || is2Pin(type); }
    static constexpr bool is2Pin(uint8_t type) { return (type >= TYPE_2PIN_MIN && type <= TYPE_2PIN_MAX); }
    static constexpr bool isOnOff(uint8_t type) { return (type == TYPE_ONOFF); }
    static constexpr bool isPWM(uint8_t type) { return (type >= TYPE_ANALOG_MIN && type <= TYPE_ANALOG_MAX); }
    static constexpr bool isVirtual(uint8_t type) { return (type >= TYPE_VIRTUAL_MIN && type <= TYPE_VIRTUAL_MAX); }
    static constexpr bool is16bit(uint8_t type) { return type == TYPE_UCS8903 || type == TYPE_UCS8904 || type == TYPE_SM16825; }
    static constexpr bool mustRefresh(uint8_t type) { return type == TYPE_TM1814; }
    static constexpr int numPWMPins(uint8_t type) { return (type - 40); }
};
namespace BusManager
{
    extern Bus bus;

    constexpr uint32_t getNumBusses() { return 1; }
    constexpr Bus* getBus(size_t b) { return &bus; }
    constexpr void setBrightness(uint32_t b) {}
    constexpr bool hasRGB() { return true; }
    constexpr bool hasWhite() { return false; }
    constexpr bool canAllShow() { return false; }
    constexpr void show() {}
    constexpr void setSegmentCCT(int i, bool b = false) {}
    constexpr int getSegmentCCT() { return 0; }
    inline uint32_t getPixelColor(int i) { return bus.getPixelColor(i); }
    inline void setPixelColor(int i, uint32_t c) { bus.setPixelColor(i, c); }
    constexpr int add(...) { return -1; }
    constexpr uint32_t memUsage() { return 0; }
};

constexpr uint32_t WLED_NUM_PINS = 40;
namespace PinManager
{
    constexpr bool isPinOk(int pin, bool b) { return true; }
    constexpr bool isPinAllocated(int pin) { return true; }
};
