
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

#include "../../settings.h"
#include "../PixelOverlayModel.h"

#include <cmath>
#include <math.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "wled.h"

#include "kiss_fftr.h"
#include "../../Warnings.h"
#include "../../mediaoutput/SDLOut.h"

uint8_t blendingStyle = 0; // effect blending/transitionig style

uint16_t rand16seed = 0;
time_t localTime = time(nullptr);
uint8_t randomPaletteChangeTime = 0;
bool stateChanged = false;
byte lastRandomIndex = 0;
char* ledmapNames[WLED_MAX_LEDMAPS] = { nullptr };
uint16_t ledMaps = 0xFFFF;
std::vector<BusConfig> busConfigs;
uint8_t errorFlag = 0;

uint8_t briS = (128);      // default brightness
uint8_t bri = (briS);      // global brightness (set)
uint8_t briOld = (0);      // global brightness while in transition loop (previous iteration)
uint8_t briT = (0);        // global brightness during transition
uint8_t briLast = (128);   // brightness before turned off. Used for toggle function
uint8_t whiteLast = (128); // white channel before turned off. Used for toggle function in ir.cpp

bool useHarmonicRandomPalette = true;
bool useGlobalLedBuffer = false;
uint8_t realtimeMode = 0;
bool realtimeRespectLedMaps = true;

char settingsPIN[5] = ""; // PIN for settings pages
bool correctPIN = 0;
unsigned long lastEditTime = 0;

FakeFL FastLED;
Bus BusManager::bus;
WLEDFileSystem WLED_FS;

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
static const char* monthShortNames_P[] = { "Err", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char* monthNames_P[] = { "Err", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
const char* monthStr(uint8_t m) {
    return monthNames_P[m];
}
const char* monthShortStr(uint8_t m) {
    return monthShortNames_P[m];
}

static const char* datStrShort_P[] = { "Err", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* datStr_P[] = { "Err", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
const char* dayStr(uint8_t day) {
    return datStr_P[day];
}
const char* dayShortStr(uint8_t day) {
    return datStrShort_P[day];
}

uint16_t XY(uint8_t x, uint8_t y) {
    return x;
}
uint32_t get_millisecond_timer() {
    return GetTimeMS();
}

// RMS average
static float fftAddAvgRMS(int from, int to, float* samples) {
    double result = 0.0;
    for (int i = from; i <= to; i++) {
        result += samples[i] * samples[i];
    }
    return sqrtf(result / float(to - from + 1));
}

static float fftAddAvg(int from, int to, float* samples) {
    if (from == to)
        return samples[from];               // small optimization
    return fftAddAvgRMS(from, to, samples); // use SMS
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

class WLEDAudioReactiveSoundSource {
public:
    WLEDAudioReactiveSoundSource() {
    }
    ~WLEDAudioReactiveSoundSource() {
        if (audioDev > 1) {
            SDL_PauseAudioDevice(audioDev, 1);
            SDL_CloseAudioDevice(audioDev);
        }
    }
    static void InputAudioCallback(WLEDAudioReactiveSoundSource* p, uint8_t* stream, int len) {
        int16_t* data = (int16_t*)stream;
        int numSamples = len / sizeof(int16_t);
        int start = 0;
        if (numSamples > NUM_SAMPLES) {
            numSamples = NUM_SAMPLES;
            start = numSamples - NUM_SAMPLES;
        }
        p->inputSamples.fill(0);
        for (int i = 0; i < numSamples; i++) {
            float f = data[start + i] / 32768.0f;
            p->inputSamples[i] = data[start + i];
        }
        /*
        float sum = 0;
        float min = 99999;
        float max = -99999;
        for (int i = 0; i < numSamples; i++) {
            sum += data[i];
            if (data[i] < min) {
                min = data[i];
            }
            if (data[i] > max) {
                max = data[i];
            }
        }
        printf("Samples: %d  Sum: %0.3f      %0.3f -> %0.3f\n", numSamples, sum, min, max);
        */
    }
    void openAudioDevice(const std::string& dev) {
        SDL_AudioSpec want;
        SDL_AudioSpec obtained;

        SDL_memset(&want, 0, sizeof(want));
        SDL_memset(&obtained, 0, sizeof(obtained));
        want.freq = 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = NUM_SAMPLES;
        want.callback = (SDL_AudioCallback)InputAudioCallback;
        want.userdata = this;
        SDL_Init(SDL_INIT_AUDIO);
        SDL_ClearError();

        audioDev = SDL_OpenAudioDevice(dev.c_str(), 1, &want, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (audioDev < 2) {
            std::string err = SDL_GetError();
            WarningHolder::AddWarning("WLED Sound Reactive - Could not open Output Audio Device: " + dev + "   Error: " + err);
            return;
        }
        SDL_ClearError();
        SDL_AudioStatus as = SDL_GetAudioDeviceStatus(audioDev);
        if (as == SDL_AUDIO_PAUSED) {
            SDL_PauseAudioDevice(audioDev, 0);
        }
        inputSampleRate = obtained.freq;
    }

    bool getAudioSamples(std::array<float, NUM_SAMPLES>& samples, int& sampleRate) {
        if (sourceType == -1) {
            std::string source = getSetting("WLEDAudioInput", "-- Playing Media --");
            if (source == "-- Playing Media --") {
                sourceType = 0;
            } else {
                sourceType = 1;
                openAudioDevice(source);
            }
        }
        bool retValue = false;
        if (sourceType == 0) {
            // Playing Media
            retValue = SDLOutput::GetAudioSamples(&samples[0], NUM_SAMPLES, sampleRate);
        } else if (sourceType == 1) {
            // Audio Input
            if (audioDev > 0) {
                samples = inputSamples;
                sampleRate = inputSampleRate;
                retValue = true;
            }
        }
        /*
            float sum = 0;
            float min = 999;
            float max = -999;
            for (int i = 0; i < NUM_SAMPLES; i++) {
                sum += inputSamples[i];
                if (inputSamples[i] < min) {
                    min = inputSamples[i];
                }
                if (inputSamples[i] > max) {
                    max = inputSamples[i];
                }
            }
            printf("ST:  %d    Sample rate:  %d    Samples: %d  Sum: %0.3f      %0.3f -> 0.3f\n", sourceType, inputSampleRate, NUM_SAMPLES, sum, min,  max);
        */
        return retValue;
    }

    int sourceType = -1;
    int audioDev = 0;
    std::array<float, NUM_SAMPLES> inputSamples;
    int inputSampleRate;

} WLED_SOUND_SOURCE;

WS2812FXExt::WS2812FXExt() :
    WS2812FX() {
    pushCurrent(this);
    finalizeInit();
    popCurrent();
}
WS2812FXExt::WS2812FXExt(PixelOverlayModel* m, int map, int b,
                         uint8_t mode, uint8_t s, uint8_t i, uint8_t pal,
                         uint32_t c1, uint32_t c2, uint32_t c3,
                         uint8_t custom1, uint8_t custom2, uint8_t custom3,
                         int check1, int check2, int check3,
                         const std::string& text) :
    WS2812FX(), model(m), mapping(map), brightness(b) {
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
    _segments[0].setMode(mode, true);
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

    _segments[0].setColor(0, c1);
    _segments[0].setColor(1, c2);
    _segments[0].setColor(2, c3);

    um_data.u_size = UDATA_ELEMENTS;
    um_data.u_type = &udata_types[0];
    um_data.u_data = &udata_ptrs[0];
    um_data.u_data[0] = &volumeSmth; //*used (New)
    um_data.u_type[0] = UMT_FLOAT;
    um_data.u_data[1] = &volumeRaw; // used (New)
    um_data.u_type[1] = UMT_UINT16;
    um_data.u_data[2] = &fftResult[0]; //*used (Blurz, DJ Light, Noisemove, GEQ_base, 2D Funky Plank, Akemi)
    um_data.u_type[2] = UMT_BYTE_ARR;
    um_data.u_data[3] = &samplePeak; //*used (Puddlepeak, Ripplepeak, Waterfall)
    um_data.u_type[3] = UMT_BYTE;
    um_data.u_data[4] = &FFT_MajorPeak; //*used (Ripplepeak, Freqmap, Freqmatrix, Freqpixels, Freqwave, Gravfreq, Rocktaves, Waterfall)
    um_data.u_type[4] = UMT_FLOAT;
    um_data.u_data[5] = &my_magnitude; // used (New)
    um_data.u_type[5] = UMT_FLOAT;
    um_data.u_data[6] = &maxVol; // assigned in effect function from UI element!!! (Puddlepeak, Ripplepeak, Waterfall)
    um_data.u_type[6] = UMT_BYTE;
    um_data.u_data[7] = &binNum; // assigned in effect function from UI element!!! (Puddlepeak, Ripplepeak, Waterfall)
    um_data.u_type[7] = UMT_BYTE;

    /*
    micDataReal = 0.0f;
    volumeRaw = 0;
    volumeSmth = 0;
    sampleAgc = 0;
    sampleAvg = 0;
    sampleRaw = 0;
    rawSampleAgc = 0;
    my_magnitude = 0;
    FFT_Magnitude = 0;
    FFT_MajorPeak = 1;
    multAgc = 1;
    // reset FFT data
    memset(fftCalc, 0, sizeof(fftCalc));
    memset(fftAvg, 0, sizeof(fftAvg));
    memset(fftResult, 0, sizeof(fftResult));
    for (int i = (init ? 0 : 1); i < NUM_GEQ_CHANNELS; i += 2)
        fftResult[i] = 16; // make a tiny pattern
    inputLevel = 128;
      autoResetPeak();
    */

    panel.push_back(p);
    pushCurrent(this);
    finalizeInit();
    popCurrent();
}

WS2812FXExt::~WS2812FXExt() {
    um_data.u_type = nullptr;
    um_data.u_data = nullptr;
}

thread_local WS2812FXExt* currentStrip = nullptr;
WS2812FXExt* WS2812FXExt::getInstance() {
    WS2812FXExt* cs = currentStrip;
    if (cs == nullptr) {
        cs = new WS2812FXExt();
        currentStrip = cs;
    }
    return cs;
}
void WS2812FXExt::clearInstance() {
    while (currentStrip) {
        WS2812FXExt* e = currentStrip;
        currentStrip = currentStrip->parent;
        delete e;
    }
}

void WS2812FXExt::pushCurrent(WS2812FXExt* e) {
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

int Bus::getLength() const {
    if (currentStrip->model) {
        return currentStrip->model->getWidth() * currentStrip->model->getHeight();
    }
    return 1;
}
uint32_t Bus::getPixelColor(int i) const {
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
void Bus::setPixelColor(int i, uint32_t c) {
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

bool UsermodManager::getUMData(um_data_t** um_data, uint8_t mod_id) {
    return false;
}
CRGB getCRGBForBand(int x, uint8_t* fftResult, int pal) {
    CRGB value;
    CHSV hsv;
    if (pal == 71) { // bit hacky to use palette id here, but don't want to litter the code with lots of different methods. TODO: add enum for palette creation type
        if (x == 1) {
            value = CRGB(fftResult[10] / 2, fftResult[4] / 2, fftResult[0] / 2);
        } else if (x == 255) {
            value = CRGB(fftResult[10] / 2, fftResult[0] / 2, fftResult[4] / 2);
        } else {
            value = CRGB(fftResult[0] / 2, fftResult[4] / 2, fftResult[10] / 2);
        }
    } else if (pal == 72) {
        int b = map(x, 0, 255, 0, 8);                                      // convert palette position to lower half of freq band
        hsv = CHSV(fftResult[b], 255, map(fftResult[b], 0, 255, 30, 255)); // pick hue
        hsv2rgb_rainbow(hsv, value);                                       // convert to R,G,B
    }
    return value;
}
void WS2812FXExt::processSamples(std::array<float, NUM_SAMPLES>& samples, int sampleRate) {
    kiss_fft_cpx fft_out[NUM_SAMPLES];
    static kiss_fftr_cfg cfg = kiss_fftr_alloc(NUM_SAMPLES, false, 0, 0);

    // GenerateSinWave(samples, sampleRate);
    // HammingWindow(samples);

    // compute fast fourier transform
    kiss_fftr(cfg, (kiss_fft_scalar*)&samples[0], fft_out);

    // arrays

    std::array<float, 17> res;
    for (int x = 0; x < 16; x++) {
        res[x] = 0;
    }
    float rate = sampleRate;
    int curBucket = 0;
    double freqnext = 440.0 * exp2f(((float)((1 + 1) * 8) - 69.0) / 12.0);
    int end = freqnext * (float)NUM_SAMPLES / rate;
    float binHzRange = rate / (float)NUM_SAMPLES;

    float maxValue = 0;
    int maxBin = 0;
    int maxBucket = 0;
    for (int bin = 0; bin < (NUM_SAMPLES / 2); ++bin) {
        if (bin > end) {
            curBucket++;
            if (curBucket == 18) {
                break;
            }
            freqnext = 440.0 * exp2f(((double)((curBucket + 1) * 8) - 69.0) / 12.0);
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

    uint8_t* fftResult = (uint8_t*)um_data.u_data[2];
    float maxFreq = (maxBin * binHzRange) + (binHzRange / 2);
    int maxIdx = maxBin;

    // printf("Max: %0.5f Hz   idx: %d\n", maxFreq, maxIdx);
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
        // printf("%d:   %0.2f  %0.2f    %d\n", x, res[x], (float)log10(res[x]), v2);
        maxV = std::max(maxV, v2);
        maxRV = std::max(maxRV, res[x]);
        fftResult[x] = v2;
    }
    // printf("\n");
    // volumeSmth = max * 127.0;

    FFT_MajorPeak = maxFreq;
    my_magnitude = maxRV;
    maxVol = maxV;
    binNum = maxBucket;

    volumeSmth = maxFreq;
    volumeRaw = volumeSmth;
    samplePeak = random8() > 250;
    if (volumeSmth < 1)
        my_magnitude = 0.001f;
}
// Currently 4 types defined, to be fine tuned and new types added
typedef enum UM_SoundSimulations {
    UMS_BeatSin = 0,
    UMS_WeWillRockYou,
    UMS_10_13,
    UMS_14_3
} um_soundSimulations_t;
um_data_t* WS2812FXExt::getAudioSamples(int simulationId) {
    std::array<float, NUM_SAMPLES> samples;
    int sampleRate = 0;
    if (WLED_SOUND_SOURCE.getAudioSamples(samples, sampleRate)) {
        processSamples(samples, sampleRate);
    } else {
        // arrays
        auto ms = GetTimeMS();

        switch (simulationId) {
        default:
        case UMS_BeatSin:
            for (int i = 0; i < 16; i++)
                fftResult[i] = beatsin8_t(120 / (i + 1), 0, 255);
            // fftResult[i] = (beatsin8_t(120, 0, 255) + (256/16 * i)) % 256;
            volumeSmth = fftResult[8];
            break;
        case UMS_WeWillRockYou:
            if (ms % 2000 < 200) {
                volumeSmth = hw_random8();
                for (int i = 0; i < 5; i++)
                    fftResult[i] = hw_random8();
            } else if (ms % 2000 < 400) {
                volumeSmth = 0;
                for (int i = 0; i < 16; i++)
                    fftResult[i] = 0;
            } else if (ms % 2000 < 600) {
                volumeSmth = hw_random8();
                for (int i = 5; i < 11; i++)
                    fftResult[i] = hw_random8();
            } else if (ms % 2000 < 800) {
                volumeSmth = 0;
                for (int i = 0; i < 16; i++)
                    fftResult[i] = 0;
            } else if (ms % 2000 < 1000) {
                volumeSmth = hw_random8();
                for (int i = 11; i < 16; i++)
                    fftResult[i] = hw_random8();
            } else {
                volumeSmth = 0;
                for (int i = 0; i < 16; i++)
                    fftResult[i] = 0;
            }
            break;
        case UMS_10_13:
            for (int i = 0; i < 16; i++)
                fftResult[i] = perlin8(beatsin8_t(90 / (i + 1), 0, 200) * 15 + (ms >> 10), ms >> 3);
            volumeSmth = fftResult[8];
            break;
        case UMS_14_3:
            for (int i = 0; i < 16; i++)
                fftResult[i] = perlin8(beatsin8_t(120 / (i + 1), 10, 30) * 10 + (ms >> 14), ms >> 3);
            volumeSmth = fftResult[8];
            break;
        }

        samplePeak = hw_random8() > 250;
        FFT_MajorPeak = 21 + (volumeSmth * volumeSmth) / 8.0f; // walk thru full range of 21hz...8200hz
        maxVol = 31;                                           // this gets feedback fro UI
        binNum = 8;                                            // this gets feedback fro UI
        volumeRaw = volumeSmth;
        my_magnitude = 10000.0f / 8.0f; // no idea if 10000 is a good value for FFT_Magnitude ???
        if (volumeSmth < 1)
            my_magnitude = 0.001f; // noise gate closed - mute
    }
    return &um_data;
}

um_data_t* simulateSound(uint8_t simulationId) {
    return strip().getAudioSamples(simulationId);
}
