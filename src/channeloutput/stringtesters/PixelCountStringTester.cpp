/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include "PixelCountStringTester.h"
#include "../../OutputMonitor.h"
#include "../../Timers.h"
#include "../../log.h"
#include "../../channeltester/ChannelTester.h"
#include "../PixelString.h"
#include "../channeloutputthread.h"

#include <algorithm>
#include <cmath>
#include <inttypes.h>

// #define PRINT_DEBUG_INFO

uint8_t* PixelCountPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t& newLen) {
    newLen = ps->m_outputBytes;
    uint8_t* data = ps->m_outputBuffer;
    uint8_t* out = ps->pixelDataStart();
    uint32_t inCh = 0;
    unsigned char clr[6];
    clr[0] = clr[2] = 0;
    clr[1] = 255;
    clr[3] = clr[5] = 0;
    clr[4] = 255;
    int clrOffset = 0;
    int curPixel = 1;
    for (auto& vs : ps->m_virtualStrings) {
        if (m_byString) {
            clrOffset = 0;
            curPixel = 1;
        }
        if (vs.receiverNum == -1) {
            for (int x = 0; x < vs.startNulls * vs.channelsPerNode(); ++x) {
                *out++ = 0;
            }
            for (int x = 0; x < vs.pixelCount; ++x) {
                if (curPixel == 50) {
                    uint8_t brightness = vs.brightnessMap[255];
                    for (int y = 0; y < vs.channelsPerNode(); ++y) {
                        *out = brightness;
                        ++out;
                        ++inCh;
                    }
                    if (++clrOffset == 3) {
                        clrOffset = 0;
                    }
                    curPixel = 1;
                } else {
                    if (vs.colorOrder.whiteOffset() == 0) {
                        ++out;
                    }
                    for (int y = 0; y < std::min(vs.channelsPerNode(), 3); ++y) {
                        *out = vs.brightnessMap[clr[y + clrOffset]];
                        out++;
                    }
                    if (vs.colorOrder.whiteOffset() == 3) {
                        ++out;
                    }
                    inCh += vs.channelsPerNode();
                    if (curPixel % 10 == 0) {
                        if (++clrOffset == 3) {
                            clrOffset = 0;
                        }
                    }
                    ++curPixel;
                }
            }
            for (int x = 0; x < vs.endNulls * vs.channelsPerNode(); ++x) {
                *out++ = 0;
            }
        } else {
            fillInSmartReceiver(ps, vs, inChannelData, inCh, out);
        }
    }
    return data;
}

PixelCountPixelStringTester PixelCountPixelStringTester::INSTANCE_BYPORT(false);
PixelCountPixelStringTester PixelCountPixelStringTester::INSTANCE_BYSTRING(true);

// Pixels lit at once while searching.  Sixteen seed pixels draw enough to
// read clearly above the sensor's noise; sixteen bullets at full white still
// draw well under the ~2A the sensors read up to.
constexpr int WINDOW = 16;
// The furthest pixel looked for on a port with nothing configured on it.
constexpr int MAX_PIXELS = 500;
constexpr int WARMUP_MS = 250;
// Frames a pattern is held before reading, then frames read.  The settle is
// long because some sensors are slow to follow: a buffered IIO source (the
// BBB capes) delivers new values about once a second, so a shorter hold read
// the previous pattern.  A measurement taken again reads for longer.
constexpr uint32_t SETTLE_FRAMES = 16;
constexpr uint32_t SAMPLE_FRAMES = 6;
constexpr uint32_t REPEAT_SAMPLE_FRAMES = 24;
// How many times an unclear measurement is taken before it is decided on what
// has been read.  A seed pixel on a noisy sensor needs most of them.
constexpr int MAX_REPEATS = 10;
// The least a window holding any pixel draws: well under one seed pixel.
constexpr float ANY_PIXEL_MA = 3.0f;
// The least uncertainty a measurement is given.  A sensor that reads in steps
// can return the same value every time, and a spread of zero is not
// certainty.
constexpr float MIN_ERROR_MA = 1.0f;

constexpr int STATE_WARMUP = 0;
constexpr int STATE_MEASURE = 1;
constexpr int STATE_DISPLAY_PIXEL = 999;

enum Phase {
    PHASE_BASE,
    PHASE_FIRST_WINDOW,
    PHASE_DOUBLING,
    PHASE_HALVING,
    PHASE_END,
    PHASE_DONE
};

static float median(std::vector<float> v) {
    if (v.empty()) {
        return 0;
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// The mean of the readings without the highest and lowest fifth: the sensors
// throw the odd reading a couple of hundred mA out.  Also the spread of the
// readings, from their median absolute deviation, as a standard deviation.
static float trimmedMean(std::vector<float> v, float& spread) {
    spread = 0;
    if (v.empty()) {
        return 0;
    }
    std::sort(v.begin(), v.end());
    float mid = v[v.size() / 2];
    std::vector<float> dev;
    for (float f : v) {
        dev.push_back(std::fabs(f - mid));
    }
    spread = 1.4826f * median(dev);
    size_t trim = v.size() / 5;
    float sum = 0;
    for (size_t x = trim; x < v.size() - trim; x++) {
        sum += v[x];
    }
    return sum / (v.size() - 2 * trim);
}

void CurrentBasedPixelCountPixelStringTester::measureWindow(PortSearch& p, int start, int len) {
    p.litStart = start;
    p.litLen = len;
}

void CurrentBasedPixelCountPixelStringTester::startGroup() {
    for (int x = 0; x < (int)ports.size(); x++) {
        PortSearch& p = ports[x];
        p.testing = OutputMonitor::INSTANCE.isPortInGroup(curGroup, x);
        p.phase = PHASE_BASE;
        p.count = -1;
        p.repeats = 0;
        p.dSum = 0;
        p.errSq = 0;
        p.spreads.clear();
        p.fullWindows.clear();
        p.samples.clear();
        p.darkSamples.clear();
        p.darkAfterSamples.clear();
        measureWindow(p, 0, 0);
    }
    state = STATE_MEASURE;
    frameInMeasure = 0;
    sampleFrames = SAMPLE_FRAMES;
    part = 0;
}

// The pixel to read next while finding the last one: either side of the
// count the last window's current suggests, then halfway between the last
// pixel known lit and the first known not to be.
static int nextProbe(int lit, int unlit, int guess) {
    if (guess - 1 > lit && guess - 1 < unlit) {
        return guess - 1;
    }
    if (guess > lit && guess < unlit) {
        return guess;
    }
    return (lit + unlit) / 2;
}

// Takes one measurement for a port, the lit current less the dark one and
// how uncertain that is, and picks what it shows next.  Returns true once
// the port's count is known.
bool CurrentBasedPixelCountPixelStringTester::step(PortSearch& p, int port, float d, float err) {
    if (p.phase == PHASE_BASE) {
        // three dark readings, for the spread of the port's sensor
        p.phase = PHASE_FIRST_WINDOW;
        measureWindow(p, 0, WINDOW);
        return false;
    }
    // A window is lit if it draws more than half a pixel's current (or, for
    // the first, any current at all).  A measurement that does not clear
    // that by twice its uncertainty is taken again, and the readings are
    // decided together.
    float threshold = p.phase == PHASE_FIRST_WINDOW ? ANY_PIXEL_MA : 0.5f * p.perPixel;
    p.dSum += d;
    p.errSq += err * err;
    ++p.repeats;
    float mean = p.dSum / p.repeats;
    float meanErr = std::sqrt(p.errSq) / p.repeats;
    if (std::fabs(mean - threshold) < 2 * meanErr && p.repeats < MAX_REPEATS) {
        LogDebug(VB_CHANNELOUT, "Pixel count: port %d phase %d lit %d+%d: %+.1f mA (mean %+.1f +/- %.1f over %d, threshold %.1f) unclear, again\n",
                port + 1, p.phase, p.litStart, p.litLen, d, mean, meanErr, p.repeats, threshold);
        return false;
    }
    bool present = mean > threshold;
    LogDebug(VB_CHANNELOUT, "Pixel count: port %d phase %d lit %d+%d: %+.1f mA (mean %+.1f +/- %.1f over %d, threshold %.1f, per pixel %.1f) %s\n",
            port + 1, p.phase, p.litStart, p.litLen, d, mean, meanErr, p.repeats, threshold, p.perPixel,
            present ? "lit" : "dark");
    p.dSum = 0;
    p.errSq = 0;
    p.repeats = 0;
    d = mean;

    // The end is in the window starting at lo: find its last pixel, one at a
    // time, starting either side of the count the window's current suggests.
    auto probe = [&]() {
        if (p.unlitPixel - p.litPixel <= 1) {
            p.count = p.unlitPixel;
            return true;
        }
        measureWindow(p, nextProbe(p.litPixel, p.unlitPixel, p.guess), 1);
        return false;
    };
    auto halve = [&]() {
        if (p.hi - p.lo <= WINDOW) {
            p.phase = PHASE_END;
            p.litPixel = p.lo; // a lit window's first pixel is there
            p.unlitPixel = p.hi;
            p.guess = p.lo + std::clamp((int)std::lround(p.loCurrent / p.perPixel), 1, p.hi - p.lo);
            return probe();
        }
        int mid = p.lo + ((p.hi - p.lo) / WINDOW / 2) * WINDOW;
        if (mid <= p.lo) {
            mid = p.lo + WINDOW;
        }
        p.phase = PHASE_HALVING;
        measureWindow(p, mid, WINDOW);
        return false;
    };
    // A window found lit proves every window before it full: the per-pixel
    // current is taken from those, not from the first window alone, which is
    // read while the port's dark current is still climbing after power-on.
    auto foundLit = [&]() {
        if (p.litStart > p.lo) {
            p.fullWindows.push_back(p.loCurrent);
            p.perPixel = median(p.fullWindows) / WINDOW;
        }
        p.lo = p.litStart;
        p.loCurrent = d;
    };

    bool done = false;
    switch (p.phase) {
    case PHASE_FIRST_WINDOW:
        if (!present) {
            p.count = 0;
            done = true;
            break;
        }
        p.perPixel = d / WINDOW;
        p.lo = 0;
        p.loCurrent = d;
        p.hi = p.maxPixels;
        p.phase = PHASE_DOUBLING;
        if (2 * WINDOW > p.maxPixels) {
            done = halve();
        } else {
            measureWindow(p, WINDOW, WINDOW);
        }
        break;
    case PHASE_DOUBLING:
        if (present) {
            foundLit();
            int next = p.lo * 2;
            if (next + WINDOW > p.maxPixels) {
                done = halve();
            } else {
                measureWindow(p, next, WINDOW);
            }
        } else {
            p.hi = p.litStart;
            done = halve();
        }
        break;
    case PHASE_HALVING:
        if (present) {
            foundLit();
        } else {
            p.hi = p.litStart;
        }
        done = halve();
        break;
    case PHASE_END:
        if (present) {
            p.litPixel = p.litStart;
        } else {
            p.unlitPixel = p.litStart;
        }
        done = probe();
        break;
    default:
        done = true;
        break;
    }
    if (done) {
        p.phase = PHASE_DONE;
        measureWindow(p, 0, 0);
        LogInfo(VB_CHANNELOUT, "Pixel count: port %d has %d pixels on the wire (%d configured, %d nulls)\n", port + 1, p.count, std::max(p.configured, 0), p.nulls);
    }
    return done;
}

void CurrentBasedPixelCountPixelStringTester::prepareTestData(int cycleCount, float percentOfCycle) {
    uint64_t now = GetMonotonicTimeMS();
    // A new test: the first call, or the cycle count starting again (each
    // test counts from 0).  Not "cycleCount == 0": the cycles can already
    // have advanced by the first frame a string asks for.
    bool newTest = (state == -1) || (cycleCount < lastCycleCount);
    lastCycleCount = cycleCount;
    if (newTest) {
        curGroup = 0;
        OutputMonitor::INSTANCE.lockToGroup(curGroup);
        SetChannelOutputRefreshRate(20);
        for (int x = 0; x < (int)ports.size(); x++) {
            OutputMonitor::INSTANCE.SetPixelCount(x, -1, -1);
            // Start each port from its configuration as it is now: the
            // configured count, nulls and search limit are worked out from
            // the string on the port's next frame.  Kept from the first run,
            // a string fixed after a count was still searched (and reported)
            // against the old configuration until fppd restarted.
            ports[x] = PortSearch();
        }
        state = STATE_WARMUP;
        stateStartMS = now;
    }
    if (state == STATE_WARMUP) {
        if (now - stateStartMS >= WARMUP_MS) {
            startGroup();
        }
        return;
    }
    if (state != STATE_MEASURE) {
        return;
    }
    if (++frameInMeasure <= SETTLE_FRAMES) {
        return;
    }
    std::vector<float> values = OutputMonitor::INSTANCE.GetPortCurrentValues();
    for (int x = 0; x < (int)ports.size(); x++) {
        if (ports[x].testing && ports[x].phase != PHASE_DONE && x < (int)values.size()) {
            (part == 0 ? ports[x].darkSamples : part == 1 ? ports[x].samples : ports[x].darkAfterSamples).push_back(values[x]);
        }
    }
    if (frameInMeasure < SETTLE_FRAMES + sampleFrames) {
        return;
    }
    frameInMeasure = 0;
    if (part < 2) {
        // dark, then the patterns lit, then dark again
        ++part;
        return;
    }
    part = 0;
    bool allDone = true;
    bool repeating = false;
    for (int x = 0; x < (int)ports.size(); x++) {
        PortSearch& p = ports[x];
        if (!p.testing || p.phase == PHASE_DONE) {
            continue;
        }
        // The lit reading less the mean of the dark ones either side of it:
        // a port's dark current climbs for a while after it powers on, and a
        // string powered at both ends does not return to the same dark
        // current once it has been lit.
        float s0, s1, s2;
        float before = trimmedMean(p.darkSamples, s0);
        float lit = trimmedMean(p.samples, s1);
        float after = trimmedMean(p.darkAfterSamples, s2);
        p.spreads.push_back(s0);
        p.spreads.push_back(s1);
        p.spreads.push_back(s2);
        float spread = median(p.spreads);
        float n0 = std::max<size_t>(p.darkSamples.size(), 1);
        float n1 = std::max<size_t>(p.samples.size(), 1);
        float n2 = std::max<size_t>(p.darkAfterSamples.size(), 1);
        // the error of the difference from the sensor's spread, and from the
        // dark current having moved while it was read
        float moved = std::fabs(after - before) / 2;
        float err = std::sqrt(spread * spread * (1 / n1 + 0.25f / n0 + 0.25f / n2) + moved * moved + MIN_ERROR_MA * MIN_ERROR_MA);
        p.base = (before + after) / 2;
        p.darkSamples.clear();
        p.samples.clear();
        p.darkAfterSamples.clear();
        if (!step(p, x, lit - p.base, err)) {
            allDone = false;
            repeating |= p.repeats > 0;
        }
    }
    sampleFrames = repeating ? REPEAT_SAMPLE_FRAMES : SAMPLE_FRAMES;
    if (!allDone) {
        return;
    }
    for (int x = 0; x < (int)ports.size(); x++) {
        if (ports[x].testing) {
            // null nodes are pixels on the wire, but not configured ones
            OutputMonitor::INSTANCE.SetPixelCount(x, std::max(ports[x].count - ports[x].nulls, 0), ports[x].configured);
        }
    }
    if (curGroup < (OutputMonitor::INSTANCE.getGroupCount() - 1)) {
        ++curGroup;
        OutputMonitor::INSTANCE.lockToGroup(curGroup);
        state = STATE_WARMUP;
        stateStartMS = now;
        for (auto& p : ports) {
            measureWindow(p, 0, 0);
        }
    } else {
        OutputMonitor::INSTANCE.lockToGroup(-1);
        state = STATE_DISPLAY_PIXEL;
        // show each string's last pixel
        for (auto& p : ports) {
            measureWindow(p, p.count > 0 ? p.count - 1 : 0, p.count > 0 ? 1 : 0);
        }
    }
}

uint8_t* CurrentBasedPixelCountPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t& newLen) {
    // PixelString allocates every string at least 2400 bytes for testers
    // like this one, and drivers have always been handed 2000 of them; a
    // string configured longer than that is searched over its own length.
    uint32_t pixelLen = std::max<int>(2000, ps->m_outputBytes - (int)ps->m_preamble.size());
    // everything below indexes from pixel 0, so work past any protocol
    // preamble and hand back the whole buffer at the end
    uint8_t* buffer = ps->pixelDataStart();
    newLen = pixelLen + ps->m_preamble.size();
    int currentPort = ps->m_portNumber;
    while ((int)ports.size() <= currentPort) {
        ports.emplace_back();
    }
    PortSearch& p = ports[currentPort];
    if (p.configured == -1) {
        p.configured = 0;
        p.nulls = 0;
        for (auto& a : ps->m_virtualStrings) {
            p.configured += a.pixelCount;
            p.nulls += a.startNulls + a.endNulls;
        }
        // Null nodes are physical pixels, sent zeros: the search counts what
        // is on the wire, and the nulls come off the count it reports.  A
        // configured port is searched a little past its configuration (enough
        // to find pixels tucked away unconfigured), not to the end of the
        // buffer: the test frame is cut there too.
        int onWire = p.configured + p.nulls;
        int cpn = ps->m_virtualStrings.empty() ? 3 : ps->m_virtualStrings[0].channelsPerNode();
        p.maxPixels = std::min(onWire > 0 ? onWire + 64 : MAX_PIXELS, (int)(pixelLen / cpn));
    }
    memset(buffer, 0, pixelLen);
    if (!ps->m_virtualStrings.empty()) {
        int cpn = ps->m_virtualStrings[0].channelsPerNode();
        newLen = std::min(pixelLen, (uint32_t)(p.maxPixels * cpn)) + ps->m_preamble.size();
    }
    if (state == STATE_WARMUP && curGroup == 0) {
        // everything dimly on for a moment first, as the outputs come up
        memset(buffer, 10, pixelLen);
    } else if (p.litLen > 0 && !(state == STATE_MEASURE && part != 1) && !ps->m_virtualStrings.empty()) {
        int cpn = ps->m_virtualStrings[0].channelsPerNode();
        int start = std::min(p.litStart, p.maxPixels);
        int len = std::min(p.litLen, p.maxPixels - start);
        if ((uint32_t)((start + len) * cpn) <= pixelLen) {
            memset(&buffer[start * cpn], 0xFF, len * cpn);
        }
    }
    return ps->m_outputBuffer;
}
CurrentBasedPixelCountPixelStringTester::Status CurrentBasedPixelCountPixelStringTester::getCurrentStatus() const {
    if (state == STATE_DISPLAY_PIXEL) {
        return CurrentBasedPixelCountPixelStringTester::Status::Complete;
    }
    if (state == -1) {
        return CurrentBasedPixelCountPixelStringTester::Status::NotRun;
    }
    return CurrentBasedPixelCountPixelStringTester::Status::Running;
}

CurrentBasedPixelCountPixelStringTester CurrentBasedPixelCountPixelStringTester::INSTANCE;
