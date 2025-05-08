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

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define WLED_DISABLE_ALEXA
#define WLED_DISABLE_MQTT
#define WLED_DISABLE_ESPNOW
#define WLED_DISABLE_HTTP
#define WLED_DISABLE_WEBSOCKETS
#define WLED_DISABLE_FTP
#define WLED_DISABLE_HTTP_AUTH
#define WLED_DISABLE_OTA
#define WLED_DISABLE_NTP
#define WLED_DISABLE_MDNS
#define WLED_DISABLE_DMX
#define WLED_DISABLE_DMX512
#define WLED_DISABLE_DMX512_SERIAL
#define WLED_DISABLE_DMX512_UNIVERSE
#define WLED_DISABLE_DMX512_UNIVERSE_SERIAL

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

#define PSTR(a) a

#include "FastLED.h"
#include "fcn_declare.h"

#include "bus_manager.h"

#include "FX.h"

class PixelOverlayModel;

typedef uint8_t fract8;
typedef uint16_t fract16;
typedef uint16_t accum88;
typedef uint8_t byte;
typedef bool boolean;

#define PROGMEM
#define IRAM_ATTR
#define USER_PRINTLN(a)
#define USER_PRINT(a)
#define pgm_read_byte(x) (*((const uint8_t*)(x)))
#define pgm_read_word(x) (*((const uint16_t*)(x)))
#define pgm_read_dword(x) (*((const uint32_t*)(x)))

inline uint8_t pgm_read_byte_near(uint8_t* p) {
    return *p;
}
inline uint8_t pgm_read_byte_near(const unsigned char* p) {
    return *p;
}

constexpr float HALF_PI = 3.14159f / 2.0f;

#define RGBW32(r, g, b, w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))

#define yield()

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

extern int hour(time_t t);   // the second for the given time
extern int minute(time_t t); // the minute for the given time
extern int second(time_t t); // the second for the given time
extern int day(time_t t);    // the day for the given time
extern int month(time_t t);  // the month for the given time
extern int year(time_t t);   // the year for the given time
extern const char* monthStr(uint8_t month);
extern const char* monthShortStr(uint8_t month);
extern const char* dayStr(uint8_t day);
extern const char* dayShortStr(uint8_t day);

#define FASTLED_RAND16_2053 ((uint16_t)(2053))
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
inline uint16_t min(uint16_t a, uint16_t b) {
    return a >= b ? b : a;
}

inline bool bitRead(uint8_t b, int position) {
    return (b >> position) & 0x1;
}
inline void bitWrite(uint8_t& b, int position, bool v) {
    if (v) {
        b |= 1 << position;
    } else {
        b &= ~(1 << position);
    }
}

#define constrain(a, b, c) ((a < b) ? b : ((a > c) ? c : a))

#define M_TWOPI 6.283185307179586476925286766559
#define radians(a) ((a * 3.14159) / 180.0)
#define DEG_TO_RAD radians(1)

#define PSTR(a) a
#define strncmp_P strncmp
#define sprintf_P sprintf
#define memcpy_P memcpy
#define snprintf_P snprintf
#define strcpy_P strcpy
#define strlen_P strlen
#define strncpy_P strncpy

#define bitClear(var, bit) ((var) &= (~(1ULL << (bit))))
#define bitSet(var, bit) ((var) |= (1ULL << (bit)))

typedef std::string String;

void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);

class WLEDFileSystem {
public:
    bool exists(...) { return false; }
};
extern WLEDFileSystem WLED_FS;

// various globals
extern uint8_t blendingStyle; // effect blending/transitionig style
extern std::vector<BusConfig> busConfigs;
extern uint8_t errorFlag;

// brightness
extern uint8_t briS;      // default brightness
extern uint8_t bri;       // global brightness (set)
extern uint8_t briOld;    // global brightness while in transition loop (previous iteration)
extern uint8_t briT;      // global brightness during transition
extern uint8_t briLast;   // brightness before turned off. Used for toggle function
extern uint8_t whiteLast; // white channel before turned off. Used for toggle function in ir.cpp

extern bool useHarmonicRandomPalette;
extern bool useGlobalLedBuffer;
extern uint8_t realtimeMode;
extern bool realtimeRespectLedMaps;

extern char settingsPIN[5]; // PIN for settings pages
extern bool correctPIN;
extern unsigned long lastEditTime;

class WS2812FXExt : public WS2812FX {
public:
    WS2812FXExt();
    WS2812FXExt(PixelOverlayModel* m, int mapping, int brightness,
                uint8_t mode, uint8_t speed, uint8_t itensity, uint8_t palette,
                uint32_t c1, uint32_t c2, uint32_t c3,
                uint8_t custom1, uint8_t custom2, uint8_t custom3,
                int check1, int check2, int check3,
                const std::string& text);

    bool setEffectConfig(uint8_t m, uint8_t s, uint8_t i, uint8_t p);
    PixelOverlayModel* model = nullptr;
    int mapping = 0;
    int brightness = 127;

    static void pushCurrent(WS2812FXExt* e);
    static void popCurrent();
    static void clearInstance();
    static WS2812FXExt* getInstance();

private:
    WS2812FXExt* parent = nullptr;
};
inline WS2812FXExt& strip() {
    return *WS2812FXExt::getInstance();
}