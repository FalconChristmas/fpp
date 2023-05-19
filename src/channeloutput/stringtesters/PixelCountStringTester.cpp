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

#include <inttypes.h>

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
constexpr int FRAMES_PER_BLOCK = 3;

uint8_t* CurrentBasedPixelCountPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) {
    newLen = 2000; // up to 500 4channel pixels
    uint8_t* buffer = ps->m_outputBuffer;

    int currentPort = ps->m_portNumber;
    if (cycleCount == 0) {
        firstPort = std::min(currentPort, firstPort);
        changeCount = 0;
        noChangeCount = 0;
        currentState = 0;
        currentTimeMS = GetTimeMS();
        startTimeMS = currentTimeMS;
        baseValues.clear();
        lastValues.clear();
        while (lastPixelIdx.size() <= currentPort) {
            lastPixelIdx.push_back(0);
            testingPort.push_back(0);
        }
        lastPixelIdx[currentPort] = -1;
        testingPort[currentPort] = true;
        if (currentPort <= lastPort) {
            SetChannelOutputRefreshRate(40);
        }
    }
    if (currentPort <= lastPort) {
        currentTimeMS = GetTimeMS();
    }
    if ((currentTimeMS - startTimeMS) < 250) {
        // turn everything on for a short time to warm up
        memset(buffer, 10, newLen);
    } else if ((currentTimeMS - startTimeMS) < 500) {
        // turn everything off
        memset(buffer, 0, newLen);
        testingPort[currentPort] = true;
        // 1/4 second to fully stabilize a baseline
        if (currentPort <= lastPort) {
            if (baseValues.size() == 0) {
                baseValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
                for (int x = 0; x < lastPixelIdx.size(); x++) {
                    OutputMonitor::INSTANCE.SetPixelCount(x, -1);
                }
            } else {
                std::vector<float> v = OutputMonitor::INSTANCE.GetPortCurrentValues();
                for (int x = 0; x < baseValues.size(); x++) {
                    baseValues[x] = std::min(baseValues[x], v[x]);
                }
            }
        }
        lastIdx = -1;
        curOutCount = 0;
    } else {
        if (currentState == 1) {
            int count = 0;
            for (auto i : lastPixelIdx) {
                if (i >= 0) {
                    count++;
                }
            }
            if (count == lastPixelIdx.size()) {
                // found the end block of all the strings
                // flip to finer attempts
                currentState = 2;
                curOutCount = 1;
            }
        } else if (currentState == 0) {
            currentState = 1;
            for (int x = 0; x < lastPixelIdx.size(); x++) {
                lastPixelIdx[x] = testingPort[x] ? -1 : 0;
            }
        }
        memset(buffer, 0, newLen);

        if (currentState == 1) {
            int cpn = ps->m_virtualStrings[0].channelsPerNode();
            int cc = curOutCount / FRAMES_PER_BLOCK;
            if (cc < 0) cc = 0;
            int idx = cc * PIXELS_PER_BLOCK;

            if (currentPort <= lastPort || lastValues.empty()) {
                curOutCount++;
                valuesIdx = lastValues.empty() ? 0 :lastIdx;
                lastValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
                for (int x = 0; x < lastValues.size(); x++) {
                    if (lastValues[x] < baseValues[x]) {
                        baseValues[x] = lastValues[x];
                    }
                }
                lastIdx = idx;

                if (idx > 500) {
                    for (int x = 0; x < lastPixelIdx.size(); x++) {
                        if (lastPixelIdx[x] == -1) {
                            //wasn't able to detect an end, set to 0
                            lastPixelIdx[x] = 0;
                        }
                    }
                    currentState = 2;
                    curOutCount = 1;
                }
            }

            if (valuesIdx >= 0 && (idx != valuesIdx )&& lastPixelIdx[currentPort] == -1 && !lastValues.empty()) {
                if (currentPort < baseValues.size()) {
                    float diff = lastValues[currentPort] - baseValues[currentPort];
                    if (diff < 15) {
                        lastPixelIdx[ps->m_portNumber] = valuesIdx == 0 ? 0 : (valuesIdx - 1);
                    }
                } else {
                    lastPixelIdx[currentPort] = 0;
                }
            }
            if (currentState == 1) {
                memset(&buffer[idx * cpn], 0xFF, PIXELS_PER_BLOCK * cpn);            
            }
        } 
        if (currentState == (11 + PIXELS_PER_BLOCK)) {
            for (int x = 0; x < lastPixelIdx.size(); x++) {
                //printf("Port %d:    %d\n", x, (lastPixelIdx[x] > 0 ?  lastPixelIdx[x] + 1 : 0));
                if (testingPort[x]) {
                    OutputMonitor::INSTANCE.SetPixelCount(x, lastPixelIdx[x] > 0 ? lastPixelIdx[x] + 1 : 0);
                }
            }
            if (currentPort == firstPort) {
                //static std::function<void()> STOP_TESTING = []() {
                //    ChannelTester::INSTANCE.StopTest();
                //};
                //Timers::INSTANCE.addTimer("Stop Pixel Count", GetTimeMS() - 50, STOP_TESTING);
                //printf("%d:  %d %d %d\n", currentState, currentPort, firstPort, lastPort);
            }
            if (lastPixelIdx[currentPort] > 0) {
                int idx = lastPixelIdx[currentPort];
                int cpn = ps->m_virtualStrings[0].channelsPerNode();
                memset(&buffer[idx * cpn], 0xFF, cpn);
            }
        } else if (currentState >= 4 && currentState < (10 + PIXELS_PER_BLOCK)) {
            int lastState = currentState;
            if (currentPort == firstPort) {
                //printf("%d    ncc: %d     cc: %d\n", currentState, noChangeCount, changeCount);
                if (currentState > 4 && changeCount == 0) {
                    if (noChangeCount == 2) {
                        currentState = (10 + PIXELS_PER_BLOCK);
                    }
                    noChangeCount++;
                } else {
                    noChangeCount = 0;
                }
                if (curOutCount == (FRAMES_PER_BLOCK + 8)) {
                    lastValues = OutputMonitor::INSTANCE.GetPortCurrentValues();
                    curOutCount = 0;
                    currentState++;
                    changeCount = 0;
                } else {
                    ++curOutCount;
                }
            }
            if (lastPixelIdx[currentPort] > 0) {
                int idx = lastPixelIdx[currentPort];
                int cpn = ps->m_virtualStrings[0].channelsPerNode();
                if (curOutCount == 0) {
                    float diff = lastValues[currentPort] - baseValues[currentPort];
                    if (diff < 12) {
                        lastPixelIdx[currentPort]--;
                        idx--;
                        changeCount++;
                    }
                }
                memset(&buffer[idx * cpn], 0xFF, cpn);
            }
        } else if (currentState > 1) {
            currentState++;
        }
    }
    lastPort = currentPort;
    return buffer;
}

CurrentBasedPixelCountPixelStringTester CurrentBasedPixelCountPixelStringTester::INSTANCE;