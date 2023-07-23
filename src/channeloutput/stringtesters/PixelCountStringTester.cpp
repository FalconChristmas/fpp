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
#include "../channeloutputthread.h"
#include "../../Timers.h"
#include "../../channeltester/ChannelTester.h"
#include "../PixelString.h"

#include <inttypes.h>

// #define PRINT_DEBUG_INFO

uint8_t* PixelCountPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) {
    newLen = ps->m_outputChannels;
    uint8_t* data = ps->m_outputBuffer;
    uint8_t* out = data;
    uint32_t inCh = 0;
    unsigned char clr[6];
    clr[0] = clr[2] = 0;
    clr[1] = 255;
    clr[3] = clr[5] = 0;
    clr[4] = 255;
    int clrOffset = cycleCount % 3;
    int curPixel = 1;
    for (auto& vs : ps->m_virtualStrings) {
        if (m_byString) {
            clrOffset = cycleCount % 3;
            curPixel = 1;
        }
        if (vs.receiverNum == -1) {
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
                    if (vs.channelsPerNode() == 4 && !vs.whiteOffset) {
                        ++out;
                    }
                    for (int y = 0; y < std::min(vs.channelsPerNode(), 3); ++y) {
                        *out = vs.brightnessMap[clr[y + clrOffset]];
                        out++;
                    }
                    if (vs.channelsPerNode() == 4 && vs.whiteOffset) {
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
        } else {
            fillInSmartReceiver(ps, vs, inChannelData, inCh, out);
        }
    }
    return data;
}

PixelCountPixelStringTester PixelCountPixelStringTester::INSTANCE_BYPORT(false);
PixelCountPixelStringTester PixelCountPixelStringTester::INSTANCE_BYSTRING(true);

constexpr int PIXELS_PER_BLOCK = 10;
constexpr int FRAMES_PER_BLOCK = 10;
constexpr int WARMUP_TIME = 250;
constexpr int BASELINE_TIME = 750;

constexpr int STATE_WARMUP = 0;
constexpr int STATE_BASELINE = 2;
constexpr int STATE_BLOCKS = 3;
constexpr int STATE_BACKOFF = 4;
constexpr int STATE_DONE_GROUP = 998;
constexpr int STATE_DISPLAY_PIXEL = 999;


void CurrentBasedPixelCountPixelStringTester::prepareTestData(int cycleCount, float percentOfCycle) {
    currentTimeMS = GetTimeMS();
    if (cycleCount == 0) {
        curGroup = 0;
        OutputMonitor::INSTANCE.lockToGroup(curGroup);
        changeCount = 0;
        noChangeCount = 0;
        currentState = 0;
        startTimeMS = currentTimeMS;
        baseValues.clear();
        lastValues.clear();
        SetChannelOutputRefreshRate(20);
    }
    if ((currentTimeMS - startTimeMS) < WARMUP_TIME) {
        currentState = STATE_WARMUP;
    } else if ((currentTimeMS - startTimeMS) < BASELINE_TIME) {
        curValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
        if (currentState != STATE_BASELINE) {
            for (int x = 0; x < testingPort.size(); x++) {
                testingPort[x] = OutputMonitor::INSTANCE.isPortInGroup(curGroup, x) ? 1 : 0;
                if (testingPort[x]) {
                    lastPixelIdx[x] = -1;
                }
            }
        }
        currentState = STATE_BASELINE;
        if (baseValues.size() == 0) {
            baseValues = curValues;
            for (int x = 0; x < lastPixelIdx.size(); x++) {
                OutputMonitor::INSTANCE.SetPixelCount(x, -1);
            }
        } else {
            for (int x = 0; x < baseValues.size(); x++) {
                if (baseValues[x] < 5) {
                    baseValues[x] = curValues[x];
                } else {
                    baseValues[x] = std::min(baseValues[x], curValues[x]);
                }
            }
#ifdef PRINT_DEBUG_INFO
            printf("%d - ", (int)(currentTimeMS - startTimeMS));
            for (int x = 0; x < baseValues.size(); x++) {
                printf("%d: %0.1f/%0.1f/%d/%d    ", x, baseValues[x], curValues[x], lastPixelIdx[x], testingPort[x]);
            }
            printf("\n");
#endif
        }
    } else if (currentState == STATE_DONE_GROUP) {
        for (int x = 0; x < testingPort.size(); x++) {
            if (testingPort[x]) {
                OutputMonitor::INSTANCE.SetPixelCount(x, lastPixelIdx[x]);
            }
        }
         if (curGroup < (OutputMonitor::INSTANCE.getGroupCount() - 1)) {
            ++curGroup;
            OutputMonitor::INSTANCE.lockToGroup(curGroup);
            cycleCount = 0;
            startTimeMS = currentTimeMS - WARMUP_TIME;
        } else {
            OutputMonitor::INSTANCE.lockToGroup(-1);
            currentState = STATE_DISPLAY_PIXEL;
        }
    } else if (currentState == STATE_DISPLAY_PIXEL) {
        
    } else if (currentState == STATE_BACKOFF) {
        ++frameInState;
        curValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
        if (frameInState == FRAMES_PER_BLOCK) {
            int changeCount = 0;
            for (int x = 0; x < testingPort.size(); x++) {
                if (testingPort[x] && lastPixelIdx[x] > 0) {
                    float diff = curValues[x] - baseValues[x];
                    if (diff < 12) {
                        lastPixelIdx[x]--;
                        changeCount++;
                    }
                }
            }
            if (changeCount == 0) {
                currentState = STATE_DONE_GROUP;
            }
            frameInState = 0;
        }
        
#ifdef PRINT_DEBUG_INFO
        printf("%d - ", currentState);
        for (int x = 0; x < baseValues.size(); x++) {
            if (testingPort[x]) {
                printf("%d: %0.1f/%0.1f/%d    ", x, baseValues[x], curValues[x], lastPixelIdx[x]);
            }
        }
        printf("\n");
#endif
    } else if (currentState == STATE_BLOCKS) {
        curValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
        
        int cc = frameInState / FRAMES_PER_BLOCK;
        int fib = frameInState % FRAMES_PER_BLOCK;
        if (cc < 0) cc = 0;
        int idx = cc * PIXELS_PER_BLOCK;

        if (fib == (FRAMES_PER_BLOCK - 1)) {
            for (int x = 0; x < curValues.size(); x++) {
                if (testingPort[x] && lastPixelIdx[x] == -1) {
                    if (curValues[x] < baseValues[x]) {
                        baseValues[x] = curValues[x];
                    }
                    float diff = curValues[x] - baseValues[x];
                    if (diff < 15) {
                        lastPixelIdx[x] = idx;
                    }
                }
            }
        }
        
        if (idx > 500) {
            for (int x = 0; x < lastPixelIdx.size(); x++) {
                if (testingPort[x] && lastPixelIdx[x] == -1) {
                    //wasn't able to detect an end, set to 0
                    lastPixelIdx[x] = 0;
                }
            }
        }
        int count = 0;
        int countTesting = 0;
        for (int x = 0; x < lastPixelIdx.size(); x++) {
            if (testingPort[x]) {
                if (lastPixelIdx[x] >= 0) {
                    count++;
                }
                countTesting++;
            }
        }
#ifdef PRINT_DEBUG_INFO
        printf("%d/%d - ", currentState, idx);
        for (int x = 0; x < baseValues.size(); x++) {
            if (testingPort[x]) {
                printf("%d: %0.1f/%0.1f/%d    ", x, baseValues[x], curValues[x], lastPixelIdx[x]);
            }
        }
        printf("\n");
#endif
        if (count == countTesting) {
            // found the end block of all the strings
            // flip to finer attempts
            currentState = STATE_BACKOFF;
            frameInState = 0;
        } else {
            ++frameInState;
        }
    } else if (currentState == STATE_BASELINE) {
        //transition out of baseline and into blocks
        curValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
        currentState = STATE_BLOCKS;
        frameInState = 0;
    } else {
        curValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
    }
}

uint8_t* CurrentBasedPixelCountPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) {
    newLen = 2000; // up to 500 4channel pixels
    uint8_t* buffer = ps->m_outputBuffer;
    int currentPort = ps->m_portNumber;

    if (currentState == STATE_WARMUP) {
        // turn everything on for a short time to warm up
        memset(buffer, 10, newLen);
        while (lastPixelIdx.size() <= currentPort) {
            lastPixelIdx.push_back(-1);
            testingPort.push_back(0);
        }
    } else if (currentState == STATE_BASELINE) {
        // turn everything off to establish a baseline
        memset(buffer, 0, newLen);
    } else if (currentState == STATE_BLOCKS) {
        memset(buffer, 0, newLen);
        if (testingPort[currentPort]) {
            int cpn = ps->m_virtualStrings[0].channelsPerNode();
            int cc = frameInState / FRAMES_PER_BLOCK;
            if (cc < 0) cc = 0;
            int idx = cc * PIXELS_PER_BLOCK;
            memset(&buffer[idx * cpn], 0xFF, PIXELS_PER_BLOCK * cpn);
        } else if (lastPixelIdx[currentPort] > 0) {
            int idx = lastPixelIdx[currentPort] - 1;
            int cpn = ps->m_virtualStrings[0].channelsPerNode();
            memset(&buffer[idx * cpn], 0xFF, cpn);
        }
    } else if (currentState == STATE_BACKOFF) {
        memset(buffer, 0, newLen);
        if (lastPixelIdx[currentPort] > 0) {
            int idx = lastPixelIdx[currentPort] - 1;
            int cpn = ps->m_virtualStrings[0].channelsPerNode();
            memset(&buffer[idx * cpn], 0xFF, cpn);
        }
    } else if (currentState == STATE_DISPLAY_PIXEL || currentState == STATE_DONE_GROUP) {
        memset(buffer, 0, newLen);
        int idx = lastPixelIdx[currentPort] - 1;
        int cpn = ps->m_virtualStrings[0].channelsPerNode();
        memset(&buffer[idx * cpn], 0xFF, cpn);
    } else {
        printf("Unknown state %d\n", currentState);
        memset(buffer, 0, newLen);
    }
    return buffer;
}

CurrentBasedPixelCountPixelStringTester CurrentBasedPixelCountPixelStringTester::INSTANCE;
