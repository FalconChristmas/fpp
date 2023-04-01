
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
#include "../../mediaoutput/SDLOut.h"
#include "kiss_fftr.h"


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

constexpr int NUM_SAMPLES = 1024;

// RMS average
static float fftAddAvgRMS(int from, int to, float *samples) {
    double result = 0.0;
    for (int i = from; i <= to; i++) {
        result += samples[i] * samples[i];
    }
    return sqrtf(result / float(to - from + 1));
}

static float fftAddAvg(int from, int to, float *samples) {
    if (from == to) return samples[from];              // small optimization
    return fftAddAvgRMS(from, to, samples);     // use SMS
}
static float mapf(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/*
static void GenerateSinWave(std::array<float, NUM_SAMPLES> &samples, float sampleRate, float frequency = 440)
{
    float sampleDurationSeconds = 1.0 / sampleRate;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        double sampleTime = (float)(i) * sampleDurationSeconds;
        samples[i] = std::sin(2.0 * 3.14159 * frequency * sampleTime);
    }
}
void HammingWindow(std::array<float, NUM_SAMPLES> &samples)
{
    static const double a0 = 25.0 / 46.0;
    static const double a1 = 1 - a0;
    for (int i = 0; i < NUM_SAMPLES; ++i)
        samples[i] *= a0 - a1 * cos((2.0 * 3.14159 * i) / NUM_SAMPLES);
}
*/

static um_data_t*processSamples(std::array<float, NUM_SAMPLES> &samples, int sampleRate) {
    static kiss_fft_cpx fft_out[NUM_SAMPLES];
    static kiss_fftr_cfg cfg = kiss_fftr_alloc(NUM_SAMPLES, false, 0, 0);
        
    //GenerateSinWave(samples, sampleRate);
    //HammingWindow(samples);

    // compute fast fourier transform
    kiss_fftr(cfg , (kiss_fft_scalar*)&samples[0], fft_out);

    static uint8_t  samplePeak;
    static float    FFT_MajorPeak;
    static uint8_t  maxVol;
    static uint8_t  binNum;
    static float    volumeSmth;
    static uint16_t volumeRaw;
    static float    my_magnitude;
    //arrays
    static um_data_t* um_data = nullptr;
    if (!um_data) {
        // initialize um_data pointer structure
        // NOTE!!!
        // This may change as AudioReactive usermod may change
        um_data = new um_data_t;
        um_data->u_size = 8;
        um_data->u_type = new um_types_t[um_data->u_size];
        um_data->u_data = new void*[um_data->u_size];
        um_data->u_data[0] = &volumeSmth;
        um_data->u_data[1] = &volumeRaw;
        um_data->u_data[2] = (uint8_t *)calloc(sizeof(uint8_t) * 16, 1);
        um_data->u_data[3] = &samplePeak;
        um_data->u_data[4] = &FFT_MajorPeak;
        um_data->u_data[5] = &my_magnitude;
        um_data->u_data[6] = &maxVol;
        um_data->u_data[7] = &binNum;
    }
    
    std::array<float, 17> res;
    for (int x = 0; x < 16; x++) {
        res[x] = 0;
    }
    float rate = sampleRate;
    int curBucket = 0;
    double freqnext = 440.0 * exp2f(((float)((1 + 1)*8) - 69.0) / 12.0);
    int end = freqnext * (float)NUM_SAMPLES / rate;
    float binHzRange = rate / (float)NUM_SAMPLES;

    float maxValue = 0;
    int maxBin = 0;
    int maxBucket = 0;
    for (int bin = 0; bin < (NUM_SAMPLES/2); ++bin) {
        if (bin > end) {
            curBucket++;
            if (curBucket == 18) {
                break;
            }
            freqnext = 440.0 * exp2f(((double)((curBucket + 1)*8) - 69.0) / 12.0);
            end = freqnext * (double)NUM_SAMPLES / rate;
        }
        float nv = sqrtf(fft_out[bin].r * fft_out[bin].r + fft_out[bin].i * fft_out[bin].i);
        if (nv > maxValue) {
            maxValue = nv;
            maxBin = bin;
            maxBucket = curBucket;
        }
        res[curBucket] = std::max(res[curBucket], nv);
    }
    
    uint8_t *fftResult =  (uint8_t*)um_data->u_data[2];
    float maxFreq = (maxBin * binHzRange) + (binHzRange / 2);
    int maxIdx = maxBin;
    
    //printf("Max: %0.5f Hz   idx: %d\n", maxFreq, maxIdx);
    int maxV = 0;
    float maxRV = 0;
    for (int x = 0; x < 16; x++) {
        int v2 = round(log10(res[x]) * 120.0);
        if (v2 > 255) {
            v2 = 255;
        }
        if (v2 < 0) {
            v2 = 0;
        }
        //printf("%d:   %0.2f  %0.2f    %d\n", x, res[x], (float)log10(res[x]), v2);
        maxV = std::max(maxV, v2);
        maxRV = std::max(maxRV, res[x]);
        fftResult[x] = v2;
    }
    //printf("\n");
    //volumeSmth = max * 127.0;
    
    FFT_MajorPeak = maxFreq;
    my_magnitude = maxRV;
    maxVol     = maxV;
    binNum     = maxBucket;

    volumeSmth = maxFreq;
    volumeRaw = volumeSmth;
    samplePeak= random8() > 250;
    if (volumeSmth < 1 ) my_magnitude = 0.001f;

    return um_data;
}


// Currently 4 types defined, to be fine tuned and new types added
typedef enum UM_SoundSimulations {
  UMS_BeatSin = 10,
  UMS_WeWillRockYou = 0,
  UMS_10_3,
  UMS_14_3
} um_soundSimulations_t;
um_data_t* simulateSound(uint8_t simulationId) {
    std::array<float, NUM_SAMPLES> samples;
    int sampleRate = 0;
    if (SDLOutput::GetAudioSamples(&samples[0], NUM_SAMPLES, sampleRate)) {
        return processSamples(samples, sampleRate);
    }
    
    static uint8_t samplePeak;
    static float   FFT_MajorPeak;
    static uint8_t maxVol;
    static uint8_t binNum;

    static float    volumeSmth;
    static uint16_t volumeRaw;
    static float    my_magnitude;

    //arrays
    uint8_t *fftResult;

    static um_data_t* um_data = nullptr;

    if (!um_data) {
      //claim storage for arrays
      fftResult = (uint8_t *)malloc(sizeof(uint8_t) * 16);

      // initialize um_data pointer structure
      // NOTE!!!
      // This may change as AudioReactive usermod may change
      um_data = new um_data_t;
      um_data->u_size = 8;
      um_data->u_type = new um_types_t[um_data->u_size];
      um_data->u_data = new void*[um_data->u_size];
      um_data->u_data[0] = &volumeSmth;
      um_data->u_data[1] = &volumeRaw;
      um_data->u_data[2] = fftResult;
      um_data->u_data[3] = &samplePeak;
      um_data->u_data[4] = &FFT_MajorPeak;
      um_data->u_data[5] = &my_magnitude;
      um_data->u_data[6] = &maxVol;
      um_data->u_data[7] = &binNum;
    } else {
      // get arrays from um_data
      fftResult =  (uint8_t*)um_data->u_data[2];
    }

    uint32_t ms = millis();

    switch (simulationId) {
      default:
      case UMS_BeatSin:
        for (int i = 0; i<16; i++)
          fftResult[i] = beatsin8(120 / (i+1), 0, 255);
          // fftResult[i] = (beatsin8(120, 0, 255) + (256/16 * i)) % 256;
          volumeSmth = fftResult[8];
        break;
      case UMS_WeWillRockYou:
        if (ms%2000 < 200) {
          volumeSmth = random8(255);
          for (int i = 0; i<5; i++)
            fftResult[i] = random8(255);
        }
        else if (ms%2000 < 400) {
          volumeSmth = 0;
          for (int i = 0; i<16; i++)
            fftResult[i] = 0;
        }
        else if (ms%2000 < 600) {
          volumeSmth = random8(255);
          for (int i = 5; i<11; i++)
            fftResult[i] = random8(255);
        }
        else if (ms%2000 < 800) {
          volumeSmth = 0;
          for (int i = 0; i<16; i++)
            fftResult[i] = 0;
        }
        else if (ms%2000 < 1000) {
          volumeSmth = random8(255);
          for (int i = 11; i<16; i++)
            fftResult[i] = random8(255);
        }
        else {
          volumeSmth = 0;
          for (int i = 0; i<16; i++)
            fftResult[i] = 0;
        }
        break;
      case UMS_10_3:
        for (int i = 0; i<16; i++)
          fftResult[i] = inoise8(beatsin8(90 / (i+1), 0, 200)*15 + (ms>>10), ms>>3);
          volumeSmth = fftResult[8];
        break;
      case UMS_14_3:
        for (int i = 0; i<16; i++)
          fftResult[i] = inoise8(beatsin8(120 / (i+1), 10, 30)*10 + (ms>>14), ms>>3);
        volumeSmth = fftResult[8];
        break;
    }

    samplePeak    = random8() > 250;
    FFT_MajorPeak = volumeSmth;
    maxVol        = 10;  // this gets feedback fro UI
    binNum        = 8;   // this gets feedback fro UI
    volumeRaw = volumeSmth;
    my_magnitude = 10000.0 / 8.0f; //no idea if 10000 is a good value for FFT_Magnitude ???
    if (volumeSmth < 1 ) my_magnitude = 0.001f;             // noise gate closed - mute

    return um_data;
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

