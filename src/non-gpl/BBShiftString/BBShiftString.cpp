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
#include "channeloutput/stringtesters/PixelStringTester.h"
#include "util/BBBUtils.h"

#include "Plugin.h"
class BBShiftStringPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin, public FPPPlugins::APIProviderPlugin {
public:
    BBShiftStringPlugin() :
        FPPPlugins::Plugin("BBShiftString") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        BBShiftStringOutput* o = new BBShiftStringOutput(startChannel, channelCount);
        outputs.push_back(o);
        return o;
    }
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {
        for (auto o : outputs) {
            o->addControlCallbacks(callbacks);
        }
    }
    std::list<BBShiftStringOutput*> outputs;
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new BBShiftStringPlugin();
}
}

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
void BBShiftStringOutput::addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {
    if (falconV5Support) {
        falconV5Support->addControlCallbacks(callbacks);
    }
}

static void compilePRUCode(const char* program, const std::string& pru, const std::vector<std::string>& sargs, const std::vector<std::string>& args1) {
    std::string log;

    char* args[sargs.size() + 4 + args1.size()];
    int idx = 0;
    args[idx++] = (char*)"/bin/bash";
    args[idx++] = (char*)program;
    args[idx++] = (char*)pru.c_str();
    log = args[1];
    log += " " + pru;
    for (int x = 0; x < sargs.size(); x++) {
        args[idx++] = (char*)sargs[x].c_str();
        log += " " + sargs[x];
    }
    for (int x = 0; x < args1.size(); x++) {
        args[idx++] = (char*)args1[x].c_str();
        log += " " + args1[x];
    }
    args[idx] = NULL;
    LogDebug(VB_CHANNELOUT, "BBShiftStringOutput::compilePRUCode() args: %s\n", log.c_str());

    pid_t compilePid = fork();
    if (compilePid == 0) {
        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

void BBShiftStringOutput::createOutputLengths(FrameData& d, const std::string& pfx) {
    union {
        uint32_t r = 0;
        uint8_t b[4];
    } r45[2];

    std::map<int, std::vector<std::tuple<int, int, GPIOCommand>>> sizes;
    int bmask = 0x1;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        for (int x = 0; x < NUM_STRINGS_PER_PIN; ++x) {
            int pc = d.stringMap[y][x];
            if (pc >= 0) {
                for (auto& a : m_strings[pc]->m_gpioCommands) {
                    sizes[a.channelOffset].push_back(std::make_tuple(y, x, a));
                }
                int breg = x % 4;
                int idx = x / 4;
                r45[idx].b[breg] |= bmask;
            }
        }
        bmask <<= 1;
    }
    d.pruData->commandTable[0] = r45[0].r;
    d.pruData->commandTable[1] = r45[1].r;
    int curCommandTable = 2;
    auto i = sizes.begin();
    while (i != sizes.end()) {
        int min = i->first;
        if (min != d.maxStringLen) {
            d.pruData->commandTable[curCommandTable++] = min;
            for (auto& t : i->second) {
                auto [y, x, cmd] = t;

                int reg = (x / 4);
                int breg = x % 4;
                uint8_t mask = 0x1 << y;
                if (cmd.type) {
                    r45[reg].b[breg] |= mask;
                } else {
                    r45[reg].b[breg] &= ~mask;
                }
            }
            d.pruData->commandTable[curCommandTable++] = r45[0].r;
            d.pruData->commandTable[curCommandTable++] = r45[1].r;
        } else {
            d.pruData->commandTable[curCommandTable] = 0xFFFF;
        }
        i++;
    }
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

    std::vector<std::string> args;
    std::vector<std::string> split0args;
    std::vector<std::string> split1args;
    split0args.push_back("-DRUNNING_ON_PRU0");
    split1args.push_back("-DRUNNING_ON_PRU1");

    m_licensedOutputs = CapeUtils::INSTANCE.getLicensedOutputs();

    config["base"] = root;

    for (int x = 0; x < m_strings.size(); x++) {
        if (m_strings[x]->m_outputChannels > 0) {
            // need to output this pin, configure it
            int pru = root["outputs"][x]["pru"].asInt();
            int pin = root["outputs"][x]["pin"].asInt();
            int pinIdx = root["outputs"][x]["index"].asInt();

            // printf("pru: %d  pin: %d  idx: %d\n", pru, pin, pinIdx);
            if (x >= m_licensedOutputs && m_strings[x]->m_outputChannels > 0) {
                // apply limit
                int pixels = 50;
                int chanCount = 0;
                for (auto& a : m_strings[x]->m_virtualStrings) {
                    if (pixels <= a.pixelCount) {
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
        }
    }
    if (hasV5SR && m_pru1.maxStringLen <= m_pru0.maxStringLen) {
        // pru1 controls the reading mux pins so it has to output more pixels than pru0 so it knows pru0 is done
        m_pru1.maxStringLen = m_pru0.maxStringLen + 1;
    }
    if (m_pru0.maxStringLen) {
        compilePRUCode("/opt/fpp/src/non-gpl/BBShiftString/compileBBShiftString.sh", "0", args, split0args);
    }
    if (m_pru1.maxStringLen) {
        compilePRUCode("/opt/fpp/src/non-gpl/BBShiftString/compileBBShiftString.sh", "1", args, split1args);
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

    if (hasV5SR) {
        constexpr int configPacketSize = 60;
        falconV5Support = new FalconV5Support();
        falconV5Support->addListeners(root["falconV5Listeners"]);

        std::vector<std::array<uint8_t, configPacketSize>> packets;
        packets.resize(m_strings.size());
        for (auto& p : packets) {
            memset(&p[0], 0, configPacketSize);
        }

        int x = 0;
        while (x < m_strings.size()) {
            int p = x;
            PixelString* p1 = m_strings[x++];
            if (p1->m_isSmartReceiver && p1->smartReceiverType == PixelString::ReceiverType::FalconV5) {
                int grp = root["outputs"][x]["falconV5Listener"].asInt();

                PixelString* p2 = x < m_strings.size() ? m_strings[x++] : nullptr;
                PixelString* p3 = x < m_strings.size() ? m_strings[x++] : nullptr;
                PixelString* p4 = x < m_strings.size() ? m_strings[x++] : nullptr;

                FalconV5Support::ReceiverChain* rc = falconV5Support->addReceiverChain(p1, p2, p3, p4, grp);
                rc->generateConfigPacket(&packets[p][0]);

                // need to keep the port on, should not turn the GPIO off
                p1->m_gpioCommands.back().type = 1;
            }
        }

        offset = ((m_pru1.frameSize / 4096) + 2) * 4096;
        // 0 position will be the config packet so its sent on first start
        m_pru0.v5_config_packets[0] = (uint8_t*)calloc(1, configPacketSize * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN);
        m_pru1.v5_config_packets[0] = (uint8_t*)calloc(1, configPacketSize * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN);

        std::array<uint8_t, configPacketSize * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN> pru0Data;
        std::array<uint8_t, configPacketSize * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN> pru1Data;

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
        bitFlipData(&pru0Data[0], m_pru0.v5_config_packets[0], 57);
        bitFlipData(&pru1Data[0], m_pru1.v5_config_packets[0], 57);
    }

    PixelString::AutoCreateOverlayModels(m_strings);
    return retVal;
}

int BBShiftStringOutput::StartPRU() {
    m_curFrame = 0;
    if (m_pru1.maxStringLen) {
        PinCapabilities::getPinByName("P8-39").configPin("pruout");
        PinCapabilities::getPinByName("P8-40").configPin("pruout");
        PinCapabilities::getPinByName("P8-41").configPin("pruout");
        PinCapabilities::getPinByName("P8-42").configPin("pruout");
        PinCapabilities::getPinByName("P8-43").configPin("pruout");
        PinCapabilities::getPinByName("P8-44").configPin("pruout");
        PinCapabilities::getPinByName("P8-45").configPin("pruout");
        PinCapabilities::getPinByName("P8-46").configPin("pruout");
        // PinCapabilities::getPinByName("P8-27").configPin("pruout");
        // PinCapabilities::getPinByName("P8-29").configPin("pruout");
        PinCapabilities::getPinByName("P8-28").configPin("pruout");
        PinCapabilities::getPinByName("P8-30").configPin("pruout");

        m_pru1.pru = new BBBPru(1, true, true);
        m_pru1.pruData = (BBShiftStringData*)m_pru1.pru->data_ram;
        m_pru1.pruData->command = 0;
        m_pru1.pruData->address_dma = m_pru1.pru->ddr_addr;
        createOutputLengths(m_pru1, "pru1");

        m_pru1.pru->run("/tmp/BBShiftString_pru1.out");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (m_pru0.maxStringLen) {
        PinCapabilities::getPinByName("P9-31").configPin("pruout");
        PinCapabilities::getPinByName("P9-29").configPin("pruout");
        PinCapabilities::getPinByName("P9-30").configPin("pruout");
        PinCapabilities::getPinByName("P9-28").configPin("pruout");
        PinCapabilities::getPinByName("P9-92").configPin("pruout");
        PinCapabilities::getPinByName("P9-27").configPin("pruout");
        PinCapabilities::getPinByName("P9-91").configPin("pruout");
        PinCapabilities::getPinByName("P9-25").configPin("pruout");
        PinCapabilities::getPinByName("P8-12").configPin("pruout");
        PinCapabilities::getPinByName("P8-11").configPin("pruout");

        m_pru0.pru = new BBBPru(0, true, true);
        m_pru0.pruData = (BBShiftStringData*)m_pru0.pru->data_ram;
        m_pru0.pruData->command = 0;
        m_pru0.pruData->address_dma = m_pru0.pru->ddr_addr;
        createOutputLengths(m_pru0, "pru0");
        m_pru0.pru->run("/tmp/BBShiftString_pru0.out");
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
    StopPRU();
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
void BBShiftStringOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType) {
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
    memcpy(d.curData, d.formattedData, d.frameSize);
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
        // memcpy(d.curData, d.formattedData, d.frameSize);
        d.pruData->address_dma = (d.pru->ddr_addr + (d.curData - d.pru->ddr));
        if (d.v5_config_packets[d.curV5ConfigPacket]) {
            memcpy(d.pru->shared_ram + d.pru->pru_num * 4096, d.v5_config_packets[d.curV5ConfigPacket], 57 * MAX_PINS_PER_PRU * NUM_STRINGS_PER_PIN);
        }
        std::swap(d.lastData, d.curData);
    }
}

int BBShiftStringOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftStringOutput::SendData(%p)\n", channelData);
    if (m_pru1.stringMap.empty() && m_pru0.stringMap.empty()) {
        return 0;
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
            c |= 0x10000; // flag a config packet needing to be sent
        }
        if (m_pru0.outputStringLen != m_pru0.maxStringLen) {
            c |= 0x20000; // flag that the output len is custom and ignore the off config
        }
        m_pru0.curV5ConfigPacket++;
        if (m_pru0.curV5ConfigPacket == NUM_CONFIG_PACKETS) {
            m_pru0.curV5ConfigPacket = 0;
        }
        m_pru0.pruData->command = c;
    }
    if (m_pru1.pruData) {
        uint32_t c = m_pru1.outputStringLen;
        if (m_pru1.v5_config_packets[m_pru1.curV5ConfigPacket]) {
            c |= 0x10000; // flag a config packet needing to be sent
        }
        if (m_pru1.outputStringLen != m_pru1.maxStringLen) {
            c |= 0x20000; // flag that the output len is custom and ignore the off config
        }
        m_pru1.curV5ConfigPacket++;
        if (m_pru1.curV5ConfigPacket == NUM_CONFIG_PACKETS) {
            m_pru1.curV5ConfigPacket = 0;
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
