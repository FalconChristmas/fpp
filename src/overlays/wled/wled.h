#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>

class PixelOverlayModel;

typedef uint8_t   fract8;
typedef uint16_t  fract16;
typedef uint16_t  accum88;
typedef uint8_t   byte;
typedef bool      boolean;

#define PROGMEM
#define IRAM_ATTR
#define USER_PRINTLN(a)
#define USER_PRINT(a)
#define DEBUG_PRINTLN(a)
#define DEBUG_PRINTF(x...)
#define pgm_read_byte(x)  (*((const  uint8_t*)(x)))
#define pgm_read_word(x)  (*((const uint16_t*)(x)))
#define pgm_read_dword(x) (*((const uint32_t*)(x)))

constexpr float HALF_PI = 3.14159f / 2.0f;


#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))

#define yield() 

long long GetTimeMS(void);
long long GetTimeMicros(void);
inline uint32_t millis() {
    return GetTimeMS();
}
inline uint32_t GET_MILLIS() {
    return GetTimeMS();
}

inline uint64_t micros() {
    return GetTimeMicros();
}

extern uint16_t rand16seed;
extern time_t localTime;
extern uint8_t randomPaletteChangeTime;
extern bool stateChanged;
extern byte lastRandomIndex;
extern char* ledmapNames[];

constexpr bool fadeTransition = false;
constexpr bool gammaCorrectCol = false;
constexpr bool useAMPM = false;
constexpr bool correctWB = false;
constexpr bool autoSegments = false;
constexpr bool gammaCorrectBri = false;
constexpr bool cctFromRgb = false;
extern uint16_t ledMaps;

extern int     hour(time_t t);  // the second for the given time
extern int     minute(time_t t);  // the minute for the given time
extern int     second(time_t t);  // the second for the given time
extern int     day(time_t t);     // the day for the given time
extern int     month(time_t t);   // the month for the given time
extern int     year(time_t t);    // the year for the given time
extern const char *monthShortStr(uint8_t m);


#define FASTLED_RAND16_2053  ((uint16_t)(2053))
#define FASTLED_RAND16_13849 ((uint16_t)(13849))
#define APPLY_FASTLED_RAND16_2053(x) (x * FASTLED_RAND16_2053)



inline uint8_t map8(uint8_t x, uint8_t in_min, uint8_t in_max, uint8_t out_min, uint8_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline uint16_t map16(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline uint16_t max(uint16_t a, uint16_t b) {
    return a >= b ? a : b;
}


inline bool bitRead(uint8_t b, int position) {
    return (b >> position) & 0x1;
}
inline void bitWrite(uint8_t &b, int position, bool v) {
    if (v) {
        b |= 1 << position;
    } else {
        b &= ~(1 << position);
    }
}

#define constrain(a, b, c) ((a < b) ? b : ((a > c) ? c : a))


#define min(a, b) (a < b ? a : b)
#define radians(a)  (( a * 3.14159 ) / 180.0)
#define DEG_TO_RAD radians(1)


#define PSTR(a) a
#define strncmp_P strncmp
#define sprintf_P sprintf
#define memcpy_P memcpy
#define snprintf_P snprintf
#define strcpy_P strcpy

uint32_t gamma32(uint32_t);


class String {
public:
    String() : data(nullptr) {}
    String(const char *c) : data(strdup(c)) {}
    ~String() { if (data) free(data);}
    
    int length() {
        return strlen(data);
    }
    bool operator==(const String&s) const {
        return strcmp(data, s.data) == 0;
    }
    String &operator+=(const String&s) {
        if (data == nullptr) {
            data = strdup(s.data);
            return *this;
        }
        char *d = (char*)malloc(strlen(data) + strlen(s.data) + 1);
        strcpy(d, data);
        strcpy(d + strlen(data), s.data);
        free(data);
        d = data;
        return *this;
    }
    char *data;
};


void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);

class BusConfig {
public:
    BusConfig(...) {}
    
};
class Bus {
public:
    int getStart() { return 0;}
    int getType() { return 0;}
    bool hasWhite() { return false; }
    bool hasRGB() { return true; }

    int getLength();
    uint32_t getPixelColor(int i);
    void setPixelColor(int i, uint32_t c);
    
    bool hasCCT() { return false; }
    bool isOk() { return true; }
    uint8_t getAutoWhiteMode() { return 0; }
    static uint8_t getGlobalAWMode() { return 0; }
    bool isOffRefreshRequired() { return false; }
};
class BusManager {
public:
    uint32_t getNumBusses() const { return 1; }
    Bus *getBus(uint32_t b) { return &bus; }
    void setBrightness(uint32_t b) {}
    bool hasRGB() { return true; }
    bool hasWhite() { return false; }
    bool canAllShow() { return false; }
    void show() {}
    void setSegmentCCT(int i, bool b = false) {}
    uint32_t getPixelColor(int i) { return bus.getPixelColor(i); }
    void setPixelColor(int i, uint32_t c) { bus.setPixelColor(i, c); }
    int add(...) {return -1;}
    
    Bus bus;
};

class WLEDFileSystem {
public:
    bool exists(...) { return false; }
};
extern WLEDFileSystem WLED_FS;
