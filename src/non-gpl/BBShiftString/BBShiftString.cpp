/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */

#include "fpp-pch.h"

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/wait.h>
#include <arm_neon.h>
#include <tuple>

#include <chrono>

// FPP includes
#include "../../Sequence.h"
#include "../../Warnings.h"
#include "../../common.h"
#include "../../log.h"

#include "BBShiftString.h"
#include "../CapeUtils/CapeUtils.h"
#include "../overlays/PixelOverlayManager.h"
#include "channeloutput/stringtesters/PixelStringTester.h"
#include "util/BBBUtils.h"

#include "Plugin.h"
class BBShiftStringPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    BBShiftStringPlugin() :
        FPPPlugins::Plugin("BBShiftString") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new BBShiftStringOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new BBShiftStringPlugin();
}
}

#ifdef PLATFORM_BBB
static const std::vector<std::string> PRU0_DATA_PINS = { "P9-31", "P9-29", "P9-30", "P9-28", "P9-92", "P9-27", "P9-91", "P9-25" };
static const std::vector<std::string> PRU0_CTRL_PINS = { "P8-12", "P8-11" };
static const std::vector<std::string> PRU1_DATA_PINS = { "P8-45", "P8-46", "P8-43", "P8-44", "P8-41", "P8-42", "P8-39", "P8-40" };
static const std::vector<std::string> PRU1_CTRL_PINS = { "P8-28", "P8-30" };
static const std::string PRU1_ENABLE_PIN = "P8-27";
#else
static const std::vector<std::string> PRU0_DATA_PINS = {};
static const std::vector<std::string> PRU0_CTRL_PINS = {};
static const std::vector<std::string> PRU1_DATA_PINS = { "P2-02", "P2-04", "P2-06", "P2-08", "P2-20", "P1-20", "P2-24", "P2-33" };
static const std::vector<std::string> PRU1_CTRL_PINS = { "P2-17", "P2-18" };
static const std::string PRU1_ENABLE_PIN = "P2-22";
#endif

static const std::vector<std::string> PRU_CTRL_PINS[2] = { PRU0_CTRL_PINS, PRU1_CTRL_PINS };
static const std::vector<std::string> PRU_DATA_PINS[2] = { PRU0_DATA_PINS, PRU1_DATA_PINS };

BBShiftStringOutput::BBShiftStringOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "BBShiftStringOutput::BBShiftStringOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
BBShiftStringOutput::~BBShiftStringOutput() {
    LogDebug(VB_CHANNELOUT, "BBShiftStringOutput::~BBShiftStringOutput()\n");
    for (auto a : m_strings) {
        delete a;
    }
    m_strings.clear();
    if (m_pru0.pru) {
        delete m_pru0.pru;
    }
    if (m_pru1.pru) {
        delete m_pru1.pru;
    }
    if (falconV5Support) {
        delete falconV5Support;
    }
}

void BBShiftStringOutput::createOutputLengths(FrameData& d, const std::string& pfx) {
    union {
        uint16_t r[2];
        uint8_t b[4];
    } r45[4];

    if (!d.pruData) {
        return;
    }
    for (int x = 0; x < 4; x++) {
        r45[x].r[0] = 0;
        r45[x].r[1] = 0;
    }
    std::map<int, std::vector<std::tuple<int, int, GPIOCommand, bool>>> sizes;
    int bmask = 0x1;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        for (int x = 0; x < NUM_STRINGS_PER_PIN; ++x) {
            int pc = d.stringMap[y][x];
            if (pc >= 0) {
                for (auto& a : m_strings[pc]->m_gpioCommands) {
                    sizes[a.channelOffset].push_back(std::make_tuple(y, x, a, m_strings[pc]->m_isInverted));
                }
                int breg = x % 4;
                int idx = x / 4;
                if (m_strings[pc]->m_isInverted) {
                    r45[idx + 2].b[breg] |= bmask;
                } else {
                    r45[idx].b[breg] |= bmask;
                }
            }
        }
        bmask <<= 1;
    }

    // need to use pru->memcpyToPRU so we'll use a temporary here
    // and it also needs to be 64 byte aligned
    uint8_t* buffer = (uint8_t*)malloc(4096 * 2);
    uintptr_t ptr = (uintptr_t)buffer;
    ptr += 64 - (ptr % 64);
    uint16_t* commandTable = (uint16_t*)ptr;

    int curCommandTable = 0;
    commandTable[curCommandTable++] = r45[0].r[0];
    commandTable[curCommandTable++] = r45[0].r[1];
    commandTable[curCommandTable++] = r45[1].r[0];
    commandTable[curCommandTable++] = r45[1].r[1];
    commandTable[curCommandTable++] = r45[2].r[0];
    commandTable[curCommandTable++] = r45[2].r[1];
    commandTable[curCommandTable++] = r45[3].r[0];
    commandTable[curCommandTable++] = r45[3].r[1];

    auto i = sizes.begin();
    while (i != sizes.end()) {
        uint16_t min = i->first & 0xFFFF;
        if (min <= d.maxStringLen) {
            commandTable[curCommandTable++] = min;
            for (auto& t : i->second) {
                auto [y, x, cmd, inverted] = t;

                int reg = (x / 4);
                int breg = x % 4;
                uint8_t mask = 0x1 << y;
                if (cmd.type) {
                    if (inverted) {
                        r45[reg].b[breg] &= ~mask;
                    } else {
                        r45[reg].b[breg] |= mask;
                    }
                } else {
                    if (inverted) {
                        r45[reg].b[breg] |= mask;
                    } else {
                        r45[reg].b[breg] &= ~mask;
                    }
                }
            }
            commandTable[curCommandTable++] = r45[0].r[0];
            commandTable[curCommandTable++] = r45[0].r[1];
            commandTable[curCommandTable++] = r45[1].r[0];
            commandTable[curCommandTable++] = r45[1].r[1];
            commandTable[curCommandTable++] = r45[2].r[0];
            commandTable[curCommandTable++] = r45[2].r[1];
            commandTable[curCommandTable++] = r45[3].r[0];
            commandTable[curCommandTable++] = r45[3].r[1];
        }
        i++;
    }
    commandTable[curCommandTable++] = 0xFFFF;
    // round up to nearest 64 byte boundary
    int len = (curCommandTable)*2;
    len += 64 - (len % 64);
    d.pru->memcpyToPRU((uint8_t*)&d.pruData->commandTable[0], (uint8_t*)&commandTable[0], len);
    free(buffer);
}

/*
 *
 */
int BBShiftStringOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "BBShiftStringOutput::Init(JSON)\n");
    std::string v;

    m_subType = config["subType"].asString();
    m_channelCount = config["channelCount"].asInt();

    Json::Value root;
    if (!CapeUtils::INSTANCE.getStringConfig(m_subType, root)) {
        LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s\n", m_subType.c_str());
        return 0;
    }

    int maxStringLen = 0;
    bool hasV5SR = false;
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s, &root["outputs"][i]))
            return 0;

        if (newString->m_outputChannels > maxStringLen) {
            maxStringLen = newString->m_outputChannels;
        }

        hasV5SR |= newString->smartReceiverType == PixelString::ReceiverType::FalconV5;
        m_strings.push_back(newString);
    }

    int retVal = ChannelOutput::Init(config);
    if (retVal == 0) {
        return 0;
    }
    if (maxStringLen == 0) {
        LogErr(VB_CHANNELOUT, "No pixels configured in any string\n");
        return 1;
    }

    m_licensedOutputs = CapeUtils::INSTANCE.getLicensedOutputs();

    config["base"] = root;

    int curRecPort = -1;
    for (int x = 0; x < m_strings.size(); x++) {
        if (m_strings[x]->smartReceiverType == PixelString::ReceiverType::FalconV5) {
            curRecPort = 0;
        }
        if (m_strings[x]->m_outputChannels > 0 || curRecPort >= 0) {
            // need to output this pin, configure it
            int pru = root["outputs"][x]["pru"].asInt();
            int pin = root["outputs"][x]["pin"].asInt();
            int pinIdx = root["outputs"][x]["index"].asInt();
            for (auto& a : PRU_CTRL_PINS[pru]) {
                if (m_usedPins.find(a) == m_usedPins.end()) {
                    m_usedPins[a] = "pru" + std::to_string(pru) + "out";
                }
            }
            const std::string& pinName = PRU_DATA_PINS[pru][pin];
            if (m_usedPins.find(pinName) == m_usedPins.end()) {
                m_usedPins[pinName] = "pru" + std::to_string(pru) + "out";
            }

            // printf("pru: %d  pin: %d  idx: %d\n", pru, pin, pinIdx);
            if (x >= m_licensedOutputs && m_strings[x]->m_outputChannels > 0) {
                // apply limit
                int pixels = 50;
                int chanCount = 0;
                for (auto& a : m_strings[x]->m_virtualStrings) {
                    if (pixels < a.pixelCount) {
                        a.pixelCount = pixels;
                    }
                    pixels -= a.pixelCount;
                    chanCount += a.pixelCount * a.channelsPerNode();
                }
                if (m_strings[x]->m_isSmartReceiver) {
                    chanCount = 0;
                }
                m_strings[x]->m_outputChannels = chanCount;
            }

            if (pru == 0) {
                m_pru0.stringMap[pin][pinIdx] = x;
                m_pru0.maxStringLen = std::max(m_pru0.maxStringLen, m_strings[x]->m_outputChannels);
            } else {
                m_pru1.stringMap[pin][pinIdx] = x;
                m_pru1.maxStringLen = std::max(m_pru1.maxStringLen, m_strings[x]->m_outputChannels);
            }
            if (++curRecPort == 4) {
                curRecPort = -1;
            }
        }
    }
    if (hasV5SR && m_pru1.maxStringLen <= m_pru0.maxStringLen) {
        // pru1 controls the reading mux pins so it has to output more pixels than pru0 so it knows pru0 is done
        m_pru1.maxStringLen = m_pru0.maxStringLen + 1;
    }

    if (!StartPRU()) {
        return 0;
    }

    // give each area two chunks (frame flipping) of DDR memory
    uint8_t* start = m_pru0.pru ? m_pru0.pru->ddr : m_pru1.pru->ddr;
    m_pru0.frameSize = NUM_STRINGS_PER_PIN * MAX_PINS_PER_PRU * std::max(2400, m_pru0.maxStringLen);
    m_pru0.curData = start;
    // leave a full memory page between to avoid conflicts
    int offset = ((m_pru0.frameSize / 4096) + 2) * 4096;
    m_pru0.lastData = m_pru0.curData + offset;

    m_pru0.channelData = (uint8_t*)calloc(1, m_pru0.frameSize);
    m_pru0.formattedData = (uint8_t*)calloc(1, m_pru0.frameSize);

    m_pru1.frameSize = NUM_STRINGS_PER_PIN * MAX_PINS_PER_PRU * std::max(2400, m_pru1.maxStringLen);
    m_pru1.curData = m_pru0.lastData + offset;
    offset = ((m_pru1.frameSize / 4096) + 2) * 4096;
    m_pru1.lastData = m_pru1.curData + offset;

    m_pru1.channelData = (uint8_t*)calloc(1, m_pru1.frameSize);
    m_pru1.formattedData = (uint8_t*)calloc(1, m_pru1.frameSize);

    bool supportsV5Listeners = root.isMember("falconV5ListenerConfig");
    if (supportsV5Listeners) {
        // if the cape supports v5 listeners, the enable pin needs to be
        // configured or data won't be sent on port1 of each receiver
        PinCapabilities::getPinByName(PRU1_ENABLE_PIN).configPin("pru1out", true, "BBShiftString-Enable");
    }
    if (hasV5SR) {
        setupFalconV5Support(root, m_pru1.lastData + offset);
    }

    PixelString::AutoCreateOverlayModels(m_strings, m_autoCreatedModelNames);
    return retVal;
}

int BBShiftStringOutput::StartPRU() {
    m_curFrame = 0;
    for (auto& a : m_usedPins) {
        PinCapabilities::getPinByName(a.first).configPin(a.second, true, "BBShiftString");
    }

    if (m_pru1.maxStringLen) {
        m_pru1.pru = new BBBPru(1, true, true);
        m_pru1.pruData = (BBShiftStringData*)m_pru1.pru->data_ram;

        m_pru1.pru->run("/opt/fpp/src/non-gpl/BBShiftString/BBShiftString_pru1.out");

        createOutputLengths(m_pru1, "pru1");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (m_pru0.maxStringLen) {
        m_pru0.pru = new BBBPru(0, true, true);
        m_pru0.pruData = (BBShiftStringData*)m_pru0.pru->data_ram;
        m_pru1.pru->run("/opt/fpp/src/non-gpl/BBShiftString/BBShiftString_pru0.out");
        createOutputLengths(m_pru0, "pru0");
    }
    return 1;
}
void BBShiftStringOutput::StopPRU(bool wait) {
    // Send the stop command
    if (m_pru0.pru) {
        m_pru0.pruData->response = 0;
        m_pru0.pruData->command = 0xFFFF;
    }
    if (m_pru1.pru) {
        m_pru1.pruData->response = 0;
        m_pru1.pruData->command = 0xFFFF;
    }
    __asm__ __volatile__("" ::
                             : "memory");

    if (m_pru1.pru) {
        int cnt = 0;
        while (wait && cnt < 25 && m_pru1.pruData->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
            __asm__ __volatile__("" ::
                                     : "memory");
        }
        m_pru1.pru->stop();
        delete m_pru1.pru;
        m_pru1.pru = nullptr;
    }

    if (m_pru0.pru) {
        int cnt = 0;
        while (wait && cnt < 25 && m_pru0.pruData->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
            __asm__ __volatile__("" ::
                                     : "memory");
        }
        m_pru0.pru->stop();
        delete m_pru0.pru;
        m_pru0.pru = nullptr;
    }
}
/*
 *
 */
int BBShiftStringOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "BBShiftStringOutput::Close()\n");
    for (auto& n : m_autoCreatedModelNames) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(n);
    }
    StopPRU();
    for (auto& a : m_usedPins) {
        PinCapabilities::getPinByName(a.first).configPin("gpio", false);
    }
    return ChannelOutput::Close();
}

void BBShiftStringOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    PixelString* ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];

        for (auto& vs : ps->m_virtualStrings) {
            int min = FPPD_MAX_CHANNELS;
            int max = -1;

            int* map = vs.chMap;
            for (int c = 0; c < vs.chMapCount; c++) {
                int ch = map[c];
                if (ch < FPPD_MAX_CHANNELS) {
                    min = std::min(min, ch);
                    max = std::max(max, ch);
                }
            }
            if (min < max) {
                addRange(min, max);
            }
        }
    }
}
void BBShiftStringOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    m_testCycle = cycleNum;
    m_testType = testType;
    m_testPercent = percentOfCycle;

    // We won't overlay the data here because we could have multiple strings
    // pointing at the same channel range so a per-port test cannot
    // be done via channel ranges.  We'll record the test information and use
    // that in prepData
}

void BBShiftStringOutput::bitFlipData(uint8_t* stringChannelData, uint8_t* bitSwapped, size_t len) {
    /*
    uint64_t *iframe = (uint64_t*)bitSwapped;
    uint64_t *buf = (uint64_t)stringChannelData;
    constexpr uint64_t mask = 0x0101010101010101ULL;
    for (int p = 0; p < len; p++) {
        memcpy(buf, iframe, MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN);
        for (int x = 0; x < 8; x++) {
            iframe[7-x] = (buf[0] >> x) & mask;
            iframe[7-x] |= ((buf[1] >> x) & mask) << 1;
            iframe[7-x] |= ((buf[2] >> x) & mask) << 2;
            iframe[7-x] |= ((buf[3] >> x) & mask) << 3;
            iframe[7-x] |= ((buf[4] >> x) & mask) << 4;
            iframe[7-x] |= ((buf[5] >> x) & mask) << 5;
            iframe[7-x] |= ((buf[6] >> x) & mask) << 6;
            iframe[7-x] |= ((buf[7] >> x) & mask) << 7;
        }
        iframe += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN / 8;
        buf += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN / 8;
    }
    */

    // NEON version of above.   On 64bit arm, the above will likely work
    // just as well.  On 32bit, the 64bit shifts above take 3 instructions
    // so if we use NEON, we can leverage the 64bit NEON registers
    uint8_t* iframe = bitSwapped;
    uint8x8_t buf[8];
    for (int p = 0; p < len; p++) {
        buf[0] = vld1_u8(&stringChannelData[0]);
        buf[1] = vld1_u8(&stringChannelData[8]);
        buf[2] = vld1_u8(&stringChannelData[16]);
        buf[3] = vld1_u8(&stringChannelData[24]);
        buf[4] = vld1_u8(&stringChannelData[32]);
        buf[5] = vld1_u8(&stringChannelData[40]);
        buf[6] = vld1_u8(&stringChannelData[48]);
        buf[7] = vld1_u8(&stringChannelData[56]);

        uint8x8_t tmp = vsli_n_u8(buf[0], buf[1], 1);
        tmp = vsli_n_u8(tmp, buf[2], 2);
        tmp = vsli_n_u8(tmp, buf[3], 3);
        tmp = vsli_n_u8(tmp, buf[4], 4);
        tmp = vsli_n_u8(tmp, buf[5], 5);
        tmp = vsli_n_u8(tmp, buf[6], 6);
        vst1_u8(&iframe[7 * 8], vsli_n_u8(tmp, buf[7], 7));

        tmp = vshr_n_u8(buf[0], 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 1), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 1), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 1), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 1), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 1), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 1), 6);
        vst1_u8(&iframe[6 * 8], vsli_n_u8(tmp, vshr_n_u8(buf[7], 1), 7));

        tmp = vshr_n_u8(buf[0], 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 2), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 2), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 2), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 2), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 2), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 2), 6);
        vst1_u8(&iframe[5 * 8], vsli_n_u8(tmp, vshr_n_u8(buf[7], 2), 7));

        tmp = vshr_n_u8(buf[0], 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 3), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 3), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 3), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 3), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 3), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 3), 6);
        vst1_u8(&iframe[4 * 8], vsli_n_u8(tmp, vshr_n_u8(buf[7], 3), 7));

        tmp = vshr_n_u8(buf[0], 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 4), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 4), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 4), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 4), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 4), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 4), 6);
        vst1_u8(&iframe[3 * 8], vsli_n_u8(tmp, vshr_n_u8(buf[7], 4), 7));

        tmp = vshr_n_u8(buf[0], 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 5), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 5), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 5), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 5), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 5), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 5), 6);
        vst1_u8(&iframe[2 * 8], vsli_n_u8(tmp, vshr_n_u8(buf[7], 5), 7));

        tmp = vshr_n_u8(buf[0], 6);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 6), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 6), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 6), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 6), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 6), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 6), 6);
        vst1_u8(&iframe[1 * 8], vsli_n_u8(tmp, vshr_n_u8(buf[7], 6), 7));

        tmp = vshr_n_u8(buf[0], 7);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[1], 7), 1);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[2], 7), 2);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[3], 7), 3);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[4], 7), 4);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[5], 7), 5);
        tmp = vsli_n_u8(tmp, vshr_n_u8(buf[6], 7), 6);
        vst1_u8(iframe, vsli_n_u8(tmp, vshr_n_u8(buf[7], 7), 7));

        iframe += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
        stringChannelData += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
    }
}

void BBShiftStringOutput::prepData(FrameData& d, unsigned char* channelData) {
    if (d.maxStringLen == 0) {
        return;
    }
    int count = 0;

    uint8_t* out = d.channelData;

    PixelString* ps = NULL;
    uint8_t* frame = NULL;
    uint8_t value;

    PixelStringTester* tester = nullptr;
    if (m_testType && m_testCycle >= 0) {
        if (m_testType == 999 && falconV5Support && m_testCycle == 0) {
            falconV5Support->sendCountPixelPackets();
        }
        tester = PixelStringTester::getPixelStringTester(m_testType);
        tester->prepareTestData(m_testCycle, m_testPercent);
    }
    uint32_t newMax = d.maxStringLen;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        uint8_t pinMask = 1 << y;
        for (int x = 0; x < NUM_STRINGS_PER_PIN; ++x) {
            int idx = d.stringMap[y][x];
            frame = out + x + (y * NUM_STRINGS_PER_PIN);
            if (idx != -1) {
                ps = m_strings[idx];
                uint32_t newLen = ps->m_outputChannels;
                uint8_t* d = tester
                                 ? tester->createTestData(ps, m_testCycle, m_testPercent, channelData, newLen)
                                 : ps->prepareOutput(channelData);
                newMax = std::max(newMax, newLen);
                for (int p = 0; p < newLen; p++) {
                    *frame = *d;
                    frame += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
                    ++d;
                }
            }
        }
    }
    d.outputStringLen = newMax;
    bitFlipData(out, d.formattedData, newMax);
    // memcpy(d.curData, d.formattedData, d.frameSize);
}

void BBShiftStringOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftStringOutput::PrepData(%p)\n", channelData);
    if (m_pru1.stringMap.empty() && m_pru0.stringMap.empty()) {
        return;
    }
    // auto start = std::chrono::high_resolution_clock::now();
    m_curFrame++;

    prepData(m_pru0, channelData);
    prepData(m_pru1, channelData);
    m_testCycle = -1;

    if (m_pru1.curV5ConfigPacket > FIRST_LOOPING_CONFIG_PACKET && falconV5Support && m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket] == nullptr) {
        std::vector<std::array<uint8_t, 64>> packets;
        packets.resize(m_strings.size());
        for (auto& p : packets) {
            memset(&p[0], 0, 64);
        }
        bool listen = false;
        if (falconV5Support->generateDynamicPacket(packets, listen)) {
            if (m_pru0.dynamicPacketInfo == &m_pru0.dynamicPacketInfo1) {
                m_pru0.dynamicPacketInfo = &m_pru0.dynamicPacketInfo2;
                m_pru1.dynamicPacketInfo = &m_pru1.dynamicPacketInfo2;
            } else {
                m_pru0.dynamicPacketInfo = &m_pru0.dynamicPacketInfo1;
                m_pru1.dynamicPacketInfo = &m_pru1.dynamicPacketInfo1;
            }
            encodeFalconV5Packet(packets, m_pru0.dynamicPacketInfo->data, m_pru1.dynamicPacketInfo->data);
            m_pru0.dynamicPacketInfo->listen = listen;
            m_pru1.dynamicPacketInfo->listen = listen;
            m_pru0.v5_config_packets[m_pru0.curV5ConfigPacket] = m_pru0.dynamicPacketInfo;
            m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket] = m_pru1.dynamicPacketInfo;
        }
    }

    /*
    uint32_t *iframe = (uint32_t*)m_pru1.curData;
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 8; y++) {
            printf("%8X ", iframe[y]);
        }
        printf("\n");
        iframe += 8;
    }
    printf("\n");
    */
    // auto finish = std::chrono::high_resolution_clock::now();
    // auto total =std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
    // printf("Total:   %lld\n", total);
}

void BBShiftStringOutput::sendData(FrameData& d) {
    if (d.outputStringLen) {
        memcpy(d.curData, d.formattedData, d.frameSize);
        // make sure memory is flushed
        msync(d.curData, d.frameSize, MS_SYNC | MS_INVALIDATE);
        __builtin___clear_cache(d.curData, d.curData + d.frameSize);

        d.pruData->address_dma = (d.pru->ddr_addr + (d.curData - d.pru->ddr));
        if (d.v5_config_packets[d.curV5ConfigPacket]) {
            d.pruData->address_dma_packet = (d.pru->ddr_addr + (d.v5_config_packets[d.curV5ConfigPacket]->data - d.pru->ddr));
        } else {
            d.pruData->address_dma_packet = 0;
        }
        std::swap(d.lastData, d.curData);
    }
}

int BBShiftStringOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftStringOutput::SendData(%p)\n", channelData);
    if (m_pru1.stringMap.empty() && m_pru0.stringMap.empty()) {
        return 0;
    }

    if (falconV5Support) {
        falconV5Support->processListenerData();
    }
    sendData(m_pru0);
    sendData(m_pru1);
    // make sure memory is flushed before command is set to 1
    __asm__ __volatile__("" ::
                             : "memory");

    // Send the start command
    if (m_pru0.pruData) {
        uint32_t c = m_pru0.outputStringLen;
        if (m_pru0.v5_config_packets[m_pru0.curV5ConfigPacket]) {
            c |= m_pru0.v5_config_packets[m_pru0.curV5ConfigPacket]->getCommandFlags();
            if (m_pru0.v5_config_packets[m_pru0.curV5ConfigPacket] == m_pru0.dynamicPacketInfo) {
                m_pru0.v5_config_packets[m_pru0.curV5ConfigPacket] = nullptr;
            }
        }
        if (m_pru0.outputStringLen != m_pru0.maxStringLen) {
            c |= 0x10000; // flag that the output len is custom and ignore the off config
        }
        m_pru0.curV5ConfigPacket++;
        if (m_pru0.curV5ConfigPacket == NUM_CONFIG_PACKETS) {
            m_pru0.curV5ConfigPacket = FIRST_LOOPING_CONFIG_PACKET;
        }
        m_pru0.pruData->command = c;
    }
    if (m_pru1.pruData) {
        uint32_t c = m_pru1.outputStringLen;
        if (m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket]) {
            c |= m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket]->getCommandFlags();
            if (m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket] == m_pru1.dynamicPacketInfo) {
                m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket] = nullptr;
            }
        }
        if (m_pru1.outputStringLen != m_pru1.maxStringLen) {
            c |= 0x10000; // flag that the output len is custom and ignore the off config
        }
        m_pru1.curV5ConfigPacket++;
        if (m_pru1.curV5ConfigPacket == NUM_CONFIG_PACKETS) {
            m_pru1.curV5ConfigPacket = FIRST_LOOPING_CONFIG_PACKET;
        }
        m_pru1.pruData->command = c;
    }
    __asm__ __volatile__("" ::
                             : "memory");

    return m_channelCount;
}

/*
 *
 */
void BBShiftStringOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "BBShiftStringOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    type          : %s\n", m_subType.c_str());
    LogDebug(VB_CHANNELOUT, "    strings       : %d\n", m_strings.size());
    LogDebug(VB_CHANNELOUT, "    longest string: %d channels\n",
             std::max(m_pru0.maxStringLen, m_pru1.maxStringLen));

    for (int i = 0; i < m_strings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "    string #%02d\n", i);
        m_strings[i]->DumpConfig();
    }

    ChannelOutput::DumpConfig();
}
void BBShiftStringOutput::StartingOutput() {
    m_pru1.curV5ConfigPacket = 0;
    m_pru0.curV5ConfigPacket = 0;
}
void BBShiftStringOutput::StoppingOutput() {
    for (int y = 0; y < NUM_CONFIG_PACKETS; ++y) {
        if (m_pru1.v5_config_packets[y] == &m_pru1.dynamicPacketInfo2 && m_pru1.v5_config_packets[y] == &m_pru1.dynamicPacketInfo1) {
            m_pru1.v5_config_packets[y] = nullptr;
        }
        if (m_pru0.v5_config_packets[y] == &m_pru0.dynamicPacketInfo2 && m_pru0.v5_config_packets[y] == &m_pru0.dynamicPacketInfo1) {
            m_pru0.v5_config_packets[y] = nullptr;
        }
    }
    m_pru1.curV5ConfigPacket = 0;
    m_pru0.curV5ConfigPacket = 0;
}
constexpr int numPacketTypes = 12;
static void invertPackets(std::array<std::array<uint8_t, 64>, numPacketTypes>& pckt) {
    for (auto& d : pckt) {
        for (int x = 0; x < 57; x++) {
            d[x] = ~d[x];
        }
    }
}
static void invertPacket(std::array<uint8_t, 64>& d) {
    for (int x = 0; x < 57; x++) {
        d[x] = ~d[x];
    }
}
void BBShiftStringOutput::encodeFalconV5Packet(std::vector<std::array<uint8_t, 64>>& packets, uint8_t* memLocPru0, uint8_t* memLocPru1) {
    int x = 0;
    while (x < m_strings.size()) {
        int p = x;
        PixelString* p1 = m_strings[x++];
        if (p1->m_isInverted) {
            invertPacket(packets[p]);
        }
    }
    std::array<uint8_t, 57 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN> pru0Data;
    std::array<uint8_t, 57 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN> pru1Data;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        uint8_t pinMask = 1 << y;
        for (int x = 0; x < NUM_STRINGS_PER_PIN; ++x) {
            int idx = m_pru0.stringMap[y][x];
            if (idx != -1) {
                uint8_t* frame = &pru0Data[x + (y * NUM_STRINGS_PER_PIN)];
                for (int p = 0; p < 57; p++) {
                    uint8_t b = packets[idx][p];
                    b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
                    *frame = b;
                    frame += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
                }
            }
            idx = m_pru1.stringMap[y][x];
            if (idx != -1) {
                uint8_t* frame = &pru1Data[x + (y * NUM_STRINGS_PER_PIN)];
                for (int p = 0; p < 57; p++) {
                    uint8_t b = packets[idx][p];
                    b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
                    *frame = b;
                    frame += MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
                }
            }
        }
    }
    size_t pLen = 57 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
    if (m_pru0.maxStringLen) {
        bitFlipData(&pru0Data[0], memLocPru0, 57);
    }
    if (m_pru1.maxStringLen) {
        bitFlipData(&pru1Data[0], memLocPru1, 57);
    }
}

void BBShiftStringOutput::setupFalconV5Support(const Json::Value& root, uint8_t* memLoc) {
    falconV5Support = new FalconV5Support();
    bool supportsV5Listeners = root.isMember("falconV5ListenerConfig");
    if (supportsV5Listeners) {
        falconV5Support->addListeners(root["falconV5ListenerConfig"]);
    }

    int max = std::max(m_pru0.maxStringLen, m_pru1.maxStringLen);
    int x = 0;
    while (x < m_strings.size()) {
        int p = x;
        PixelString* p1 = m_strings[x++];
        if (p1->m_isSmartReceiver && p1->smartReceiverType == PixelString::ReceiverType::FalconV5) {
            int grp = 0;
            int mux = 0;
            if (root["outputs"][p].isMember("falconV5Listener")) {
                grp = root["outputs"][p]["falconV5Listener"].asInt();
            }
            if (root["outputs"][p].isMember("falconV5ListenerMux")) {
                mux = root["outputs"][p]["falconV5ListenerMux"].asInt();
            }
            PixelString* p2 = x < m_strings.size() ? m_strings[x++] : nullptr;
            PixelString* p3 = x < m_strings.size() ? m_strings[x++] : nullptr;
            PixelString* p4 = x < m_strings.size() ? m_strings[x++] : nullptr;

            falconV5Support->addReceiverChain(p1, p2, p3, p4, grp, mux);
            // need to keep the ports on.  With v5, all ports need to be kept on
            // and must have edges that are aligned.  Need to turn OFF the 2-4 ports during
            // the config packet
            p1->m_gpioCommands.clear();
            if (p2) {
                p2->m_gpioCommands.clear();
                p2->m_gpioCommands.emplace_back(2, max, 0, 0);
            }
            if (p3) {
                p3->m_gpioCommands.clear();
                p3->m_gpioCommands.emplace_back(3, max, 0, 0);
            }
            if (p4) {
                p4->m_gpioCommands.clear();
                p4->m_gpioCommands.emplace_back(4, max, 0, 0);
            }
        }
    }
    if (!falconV5Support->getReceiverChains().empty()) {
        // we have receiver chains, generate standard packets
        for (int x = 0; x < 3; x++) {
            std::vector<std::array<uint8_t, 64>> packets;
            std::vector<std::array<uint8_t, 64>> packets2;
            packets.resize(m_strings.size());
            packets2.resize(m_strings.size());
            for (auto& p : packets) {
                memset(&p[0], 0, 64);
            }
            for (auto& p : packets2) {
                memset(&p[0], 0, 64);
            }
            int len = 0;
            int idx = 0;
            bool listen = false;
            for (auto& rc : falconV5Support->getReceiverChains()) {
                int port = rc->getPixelStrings()[0]->m_portNumber;
                if (x == 0) {
                    rc->generateConfigPacket(&packets[port][0]);
                    idx = 0;
                    len = 1;
                } else if (x == 1) {
                    rc->generateNumberPackets(&packets[port][0], &packets2[port][0]);
                    idx = 1;
                    len = 2;
                } else if (x == 2) {
                    rc->generateSetFusesPacket(&packets[port][0], 1);
                    idx = 2;
                    len = 1;
                }
            }
            if (m_pru0.v5_config_packets[idx] == nullptr) {
                m_pru0.v5_config_packets[idx] = new FalconV5PacketInfo(len, memLoc, listen);
                // 64 to keep on 4K memory alignment
                memLoc += 64 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN * len;
                m_pru1.v5_config_packets[idx] = new FalconV5PacketInfo(len, memLoc, listen);
                memLoc += 64 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN * len;

                encodeFalconV5Packet(packets, m_pru0.v5_config_packets[idx]->data, m_pru1.v5_config_packets[idx]->data);
                if (len == 2) {
                    size_t pLen = 57 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
                    encodeFalconV5Packet(packets2, m_pru0.v5_config_packets[idx]->data + pLen, m_pru1.v5_config_packets[idx]->data + pLen);
                }

                if (idx == 0) {
                    // config packet, output in other spots
                    idx += 12;
                    while (idx < NUM_CONFIG_PACKETS) {
                        m_pru0.v5_config_packets[idx] = new FalconV5PacketInfo(len, m_pru0.v5_config_packets[0]->data, listen);
                        m_pru1.v5_config_packets[idx] = new FalconV5PacketInfo(len, m_pru1.v5_config_packets[0]->data, listen);
                        idx += 12;
                    }
                } else if (idx == 1) {
                    // number packet, resend once every loop
                    int nidx = NUM_CONFIG_PACKETS - 1;
                    while (m_pru0.v5_config_packets[nidx] != nullptr) {
                        --nidx;
                    }
                    m_pru0.v5_config_packets[nidx] = new FalconV5PacketInfo(len, m_pru0.v5_config_packets[idx]->data, listen);
                    m_pru1.v5_config_packets[nidx] = new FalconV5PacketInfo(len, m_pru1.v5_config_packets[idx]->data, listen);
                }
            }
        }
    }
    // reserve space for the dynamic packets
    m_pru0.dynamicPacketInfo1.data = memLoc;
    memLoc += 64 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
    m_pru0.dynamicPacketInfo2.data = memLoc;
    memLoc += 64 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
    m_pru1.dynamicPacketInfo1.data = memLoc;
    memLoc += 64 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;
    m_pru1.dynamicPacketInfo2.data = memLoc;
    memLoc += 64 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN;

    m_pru0.dynamicPacketInfo = &m_pru0.dynamicPacketInfo1;
    m_pru1.dynamicPacketInfo = &m_pru1.dynamicPacketInfo1;

    createOutputLengths(m_pru0, "pru0");
    createOutputLengths(m_pru1, "pru1");
}
