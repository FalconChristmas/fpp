#pragma once
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

#include "PixelStringTester.h"
#include <cstdint>
#include <vector>

class PixelCountPixelStringTester : public PixelStringTester {
protected:
    PixelCountPixelStringTester(bool byString) :
        PixelStringTester(), m_byString(byString) {}

    const bool m_byString;

public:
    virtual uint8_t* createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t& newLen) override;

    static PixelCountPixelStringTester INSTANCE_BYSTRING;
    static PixelCountPixelStringTester INSTANCE_BYPORT;
};

// Counts the pixels on each port from its current sensor.
//
// One sensor group at a time (OutputMonitor::lockToGroup), every port in the
// group searched in parallel.  Each measurement reads the port dark, then
// with a pattern lit, then dark again, and uses the lit reading less the
// mean of the dark ones: a port's dark current climbs for a while after it
// powers on, and a string powered at both ends does not return to the same
// dark current once lit.  Pixels are only ever lit in windows of WINDOW at
// full white, or one at a time at the very end: enough current for a seed
// pixel to register, too little to reach the top of the sensor's range.
//
// A window is lit if it draws more than half what a pixel does, taken from
// the windows the search has proven full, so nothing depends on a fixed mA
// threshold.  A measurement is only decided once it clears that by twice its
// uncertainty, worked out from the spread of the port's readings and how far
// the dark current moved; until then it is taken again.  A seed pixel draws
// less than a noisy sensor's spread, so the readings, not a fixed number of
// them, decide how long a port takes.
//
// The end is found by doubling then halving the window position, and then
// pixel by pixel inside the last window, starting where its current puts
// the end.  Null nodes are counted as the pixels they are, and taken off the
// count reported.
class CurrentBasedPixelCountPixelStringTester : public PixelStringTester {
public:
    static CurrentBasedPixelCountPixelStringTester INSTANCE;

    enum Status {
        NotRun,
        Running,
        Complete
    };
    Status getCurrentStatus() const;

protected:
    CurrentBasedPixelCountPixelStringTester() :
        PixelStringTester() {}

    struct PortSearch {
        int configured = -1; // pixels, as configured
        int nulls = 0;       // null nodes configured on the port
        int maxPixels = 0;   // the furthest this port is searched, on the wire
        bool testing = false;
        int phase = 0;
        float base = 0;      // mA, port dark
        float perPixel = 0;  // mA one pixel adds at full white
        std::vector<float> fullWindows; // windows a later lit one proved full
        std::vector<float> spreads;     // of each reading, for the sensor's noise
        int lo = 0;          // a window start known to hold pixels
        int hi = 0;          // a window start known to hold none
        float loCurrent = 0;
        int litPixel = 0;    // finding the last pixel: one known lit...
        int unlitPixel = 0;  // ...one known not...
        int guess = 0;       // ...and where the current puts the end
        int count = -1;      // the answer, once known
        int repeats = 0;     // readings of an unclear measurement so far
        float dSum = 0;
        float errSq = 0;
        int litStart = 0;    // what this port shows now
        int litLen = 0;
        std::vector<float> samples;
        std::vector<float> darkSamples;      // before the lit reading
        std::vector<float> darkAfterSamples; // and after it
    };
    std::vector<PortSearch> ports;

    int state = -1;
    int lastCycleCount = 0;
    int curGroup = 0;
    uint32_t frameInMeasure = 0;
    uint32_t sampleFrames = 0;
    // each measurement reads the port dark, lit, then dark again
    int part = 0;
    uint64_t stateStartMS = 0;

    void startGroup();
    bool step(PortSearch& p, int port, float d, float err);
    void measureWindow(PortSearch& p, int start, int len);

public:
    virtual void prepareTestData(int cycleCount, float percentOfCycle) override;
    virtual uint8_t* createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t& newLen) override;
};
