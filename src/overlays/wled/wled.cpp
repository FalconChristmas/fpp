
/*
 * Pixel Overlay Effects for Falcon Player (FPP) ported from WLED
 *
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

#include "fpp-pch.h"

#include <time.h>

#include "FX.h"
#include "wled.h"
#include "fcn_declare.h"

#include "../PixelOverlayModel.h"

uint16_t rand16seed = 0;
time_t localTime = time(nullptr);
uint8_t randomPaletteChangeTime = 0;
bool stateChanged = false;
byte lastRandomIndex = 0;
char* ledmapNames[WLED_MAX_LEDMAPS] = {nullptr};
uint16_t ledMaps = 0xFFFF;

FakeFL FastLED;
BusManager busses;
WLEDFileSystem WLED_FS;
Usermods usermods;

int hour(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_hour;
}
int minute(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_min;
}
int second(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_sec;
}
int day(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_mday;
}
int month(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_mon;
}
int year(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_year;
}
static const char *monthShortNames_P[] = {"Err", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char *monthShortStr(uint8_t m) {
    return monthShortNames_P[m];
}

uint16_t crc16(const unsigned char* data_p, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;
    if (!length) return 0x1D0F;
    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}
uint16_t XY(uint8_t x, uint8_t y) {
    return x;
}
uint32_t get_millisecond_timer() {
    return GetTimeMS();
}

um_data_t* simulateSound(uint8_t simulationId) {
    return nullptr;
}
CRGB getCRGBForBand(int x, uint8_t *fftResult, int pal) {
    CRGB value;
    CHSV hsv;
    if(pal == 71) { // bit hacky to use palette id here, but don't want to litter the code with lots of different methods. TODO: add enum for palette creation type
        if(x == 1) {
            value = CRGB(fftResult[10]/2, fftResult[4]/2, fftResult[0]/2);
        } else if(x == 255) {
            value = CRGB(fftResult[10]/2, fftResult[0]/2, fftResult[4]/2);
        } else {
            value = CRGB(fftResult[0]/2, fftResult[4]/2, fftResult[10]/2);
        }
    } else if(pal == 72) {
        int b = map(x, 0, 255, 0, 8); // convert palette position to lower half of freq band
        hsv = CHSV(fftResult[b], 255, map(fftResult[b], 0, 255, 30, 255));  // pick hue
        hsv2rgb_rainbow(hsv, value);  // convert to R,G,B
    }
    return value;
}
int16_t extractModeDefaults(uint8_t mode, const char *segVar) {
    if (mode < strip().getModeCount()) {
        char lineBuffer[128] = "";
        strncpy(lineBuffer, strip().getModeData(mode), 127);
        lineBuffer[127] = '\0'; // terminate string
        if (lineBuffer[0] != 0) {
            char* startPtr = strrchr(lineBuffer, ';'); // last ";" in FX data
            if (!startPtr) return -1;
            
            char* stopPtr = strstr(startPtr, segVar);
            if (!stopPtr) return -1;

            stopPtr += strlen(segVar) +1; // skip "="
            return atoi(stopPtr);
        }
    }
    return -1;
}

void WS2812FX::loadCustomPalettes() {
}

bool WS2812FX::deserializeMap(uint8_t n) {
    return false;
}


WS2812FXExt::WS2812FXExt() : WS2812FX() {
    pushCurrent(this);
    finalizeInit();
    popCurrent();
}
WS2812FXExt::WS2812FXExt(PixelOverlayModel* m, int map, int b,
                         uint8_t mode, uint8_t s, uint8_t i, uint8_t pal,
                         uint32_t c1, uint32_t c2, uint32_t c3,
                         uint8_t custom1, uint8_t custom2, uint8_t custom3,
                         int check1, int check2, int check3,
                         const std::string &text) : WS2812FX(), model(m), mapping(map), brightness(b) {
    WS2812FXExt::pushCurrent(this);
    if (m->getWidth() > 1 && m->getHeight() > 1) {
        isMatrix = true;
    }
    Panel p;
    if (mapping == 0 || mapping == 2) {
        _segments.push_back(Segment(0, m->getWidth(), 0, m->getHeight()));
        p.width = m->getWidth();
        p.height = m->getHeight();
    } else {
        _segments.push_back(Segment(0, m->getHeight(), 0, m->getWidth()));
        p.width = m->getHeight();
        p.height = m->getWidth();
    }
    milliampsPerLed = 0; //need to turn off the power calculation
    _segments[0].transitional = false;
    setMode(0, mode);
    _segments[0].speed = s;
    _segments[0].intensity = i;
    _segments[0].palette = pal;
    _segments[0].custom1 = custom1;
    _segments[0].custom2 = custom2;
    _segments[0].custom3 = custom3;
    _segments[0].check1 = check1;
    _segments[0].check2 = check2;
    _segments[0].check3 = check3;
    if (text != "") {
        _segments[0].name = new char[text.length() + 1];
        strncpy(_segments[0].name, text.c_str(), text.length() + 1);
    }
    _segments[0].refreshLightCapabilities();

    setColor(0, c1);
    setColor(1, c2);
    setColor(2, c3);


    panel.push_back(p);
    pushCurrent(this);
    finalizeInit();
    popCurrent();
}



thread_local WS2812FXExt* currentStrip = nullptr;
WS2812FXExt &strip() {
    WS2812FXExt *cs = currentStrip;
    if (cs == nullptr) {
        cs = new WS2812FXExt();
        currentStrip = cs;
    }
    return *cs;
}
void WS2812FXExt::pushCurrent(WS2812FXExt *e) {
    if (e) {
        e->parent = currentStrip;
        currentStrip = e;
    } else if (currentStrip) {
        currentStrip = currentStrip->parent;
    }
}
void WS2812FXExt::popCurrent() {
    if (currentStrip) {
        currentStrip = currentStrip->parent;
    }
}



int Bus::getLength() {
    if (currentStrip->model) {
        return currentStrip->model->getWidth() * currentStrip->model->getHeight();
    }
    return 1;
}
uint32_t Bus::getPixelColor(int i) {
    int x, y;
    int w = currentStrip->model->getWidth();
    int h = currentStrip->model->getHeight();
    int mapping = currentStrip->mapping;
    if (mapping == 1 || mapping == 3) {
        y = i % h;
        x = i / h;
    } else {
        x = i % w;
        y = i / w;
    }
    
    if (mapping == 2) {
        y = h - y - 1;
    }
    if (mapping == 3) {
        x = w - x - 1;
    }
    
    int r, g, b;
    currentStrip->model->getOverlayPixelValue(x, y, r, g, b);
    return (r << 16) | (g << 8) | b;
}
void  Bus::setPixelColor(int i, uint32_t c) {
    int x, y;
    int w = currentStrip->model->getWidth();
    int h = currentStrip->model->getHeight();
    int mapping = currentStrip->mapping;
    if (mapping == 1 || mapping == 3) {
        y = i % h;
        x = i / h;
    } else {
        x = i % w;
        y = i / w;
    }
    
    if (mapping == 2) {
        y = h - y - 1;
    }
    if (mapping == 3) {
        x = w - x - 1;
    }
    int r = R(c);
    int g = G(c);
    int b = B(c);
    int brightness = currentStrip->brightness;
    r = r * brightness / 128;
    g = g * brightness / 128;
    b = b * brightness / 128;
    currentStrip->model->setOverlayPixelValue(x, y, min(r, 255), min(g, 255), min(b, 255));
}

