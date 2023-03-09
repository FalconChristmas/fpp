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

#include <sys/wait.h>

#define BBB_PRU 1

//  #define PRINT_STATS

// FPP includes
#include "BBB48String.h"
#include "../CapeUtils/CapeUtils.h"
#include "channeloutput/stringtesters/PixelStringTester.h"
#include "util/BBBUtils.h"

#include "Plugin.h"
class BBB48StringPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    BBB48StringPlugin() :
        FPPPlugins::Plugin("BBB48String") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new BBB48StringOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new BBB48StringPlugin();
}
}

/*
 *
 */
BBB48StringOutput::BBB48StringOutput(unsigned int startChannel,
                                     unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_curFrame(0),
    m_pru(NULL),
    m_pruData(NULL),
    m_pru0(NULL),
    m_pru0Data(NULL),
    m_stallCount(0) {
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::BBB48StringOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
BBB48StringOutput::~BBB48StringOutput() {
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::~BBB48StringOutput()\n");
    m_strings.clear();
    if (m_pru)
        delete m_pru;
    if (m_pru0)
        delete m_pru0;
}

static void compilePRUCode(const char* program, const std::vector<std::string>& sargs, const std::vector<std::string>& args1) {
    std::string log;

    char* args[sargs.size() + 3 + args1.size()];
    int idx = 0;
    args[idx++] = (char*)"/bin/bash";
    args[idx++] = (char*)program;
    log = args[1];
    for (int x = 0; x < sargs.size(); x++) {
        args[idx++] = (char*)sargs[x].c_str();
        log += " " + sargs[x];
    }
    for (int x = 0; x < args1.size(); x++) {
        args[idx++] = (char*)args1[x].c_str();
        log += " " + args1[x];
    }
    args[idx] = NULL;
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::compilePRUCode() args: %s\n", log.c_str());

    pid_t compilePid = fork();
    if (compilePid == 0) {
        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

static void compilePRUCode(const std::vector<std::string>& sargs,
                           const std::vector<std::string>& args0,
                           const std::vector<std::string>& args1,
                           bool split) {
    compilePRUCode("/opt/fpp/src/non-gpl/BBB48String/compileWS2811x.sh", sargs, args1);
    if (split) {
        compilePRUCode("/opt/fpp/src/non-gpl/BBB48String/compileWS2811x_gpio0.sh", sargs, args0);
    }
}

inline int mapSize(int max, int maxString) {
    if (maxString > max) {
        maxString = max;
    }
    int newHeight = maxString;
    //make sure it's a multiple of 4 to keep things aligned in memory
    if (maxString == 0) {
        return 0;
    } else if (maxString <= 4) {
        newHeight = 4;
    } else if (maxString <= 8) {
        newHeight = 8;
    } else if (maxString <= 12) {
        newHeight = 12;
    } else if (maxString <= 16) {
        newHeight = 16;
    } else if (maxString <= 20) {
        newHeight = 20;
    } else if (maxString <= 24) {
        newHeight = 24;
    } else if (maxString <= 28) {
        newHeight = 28;
    } else if (maxString <= 32) {
        newHeight = 32;
    } else if (maxString <= 36) {
        newHeight = 36;
    } else if (maxString <= 40) {
        newHeight = 40;
    } else if (maxString <= 44) {
        newHeight = 44;
    } else {
        newHeight = 48;
    }
    return newHeight;
}

void BBB48StringOutput::createOutputLengths(FrameData& d, const std::string& pfx, std::vector<std::string>& args) {
    if (d.gpioStringMap.empty()) {
        return;
    }
    std::ofstream outputFile;
    outputFile.open("/tmp/OutputLengths" + pfx + ".hp", std::ofstream::out | std::ofstream::trunc);

    std::map<int, std::vector<GPIOCommand>> sizes;
    for (int x : d.gpioStringMap) {
        if (x != -1) {
            int pc = m_strings[x]->m_outputChannels;
            if (pc != 0) {
                for (auto& a : m_strings[x]->m_gpioCommands) {
                    sizes[a.channelOffset].push_back(a);
                }
            }
        }
    }

    auto i = sizes.begin();
    while (i != sizes.end()) {
        int min = i->first;
        outputFile << "\nCHECK_" << pfx << std::to_string(min) << ":\n";
        if (min != d.maxStringLen) {
            if (min <= 255) {
                outputFile << "    QBNE skip_"
                           << pfx
                           << std::to_string(min)
                           << ", cur_data, "
                           << std::to_string(min)
                           << "\n";
            } else {
                if (min <= 0xFFFF) {
                    outputFile << "    LDI r8, " << std::to_string(min) << "\n";
                } else {
                    outputFile << "    LDI32 r8, " << std::to_string(min) << "\n";
                }
                outputFile << "    QBNE skip_" << pfx
                           << std::to_string(min)
                           << ", cur_data, r8\n";
            }

            for (auto& cmd : i->second) {
                int y = cmd.port;
                for (int x = 0; x < d.gpioStringMap.size(); x++) {
                    if (d.gpioStringMap[x] == y) {
                        y = x;
                        break;
                    }
                }

                std::string o = std::to_string(y + 1);
                if (cmd.type) {
                    outputFile << "        SET GPIO_MASK(o" << o << "_gpio), GPIO_MASK(o" << o << "_gpio), o" << o << "_pin\n";
                } else {
                    outputFile << "        CLR GPIO_MASK(o" << o << "_gpio), GPIO_MASK(o" << o << "_gpio), o" << o << "_pin\n";
                }
            }
            i++;
            int next = i->first;
            outputFile << "        LDI next_check, $CODE(CHECK_" << pfx << std::to_string(next) << ")\n";
            outputFile << "skip_" << pfx << std::to_string(min) << ":\n        RET\n";

        } else {
            outputFile << "    RET\n\n";
            i++;
        }
    }

    if (sizes.empty()) {
        args.push_back("-DFIRST_CHECK=NO_PIXELS_CHECK");
    } else {
        int sz = sizes.begin()->first;
        std::string v = "-DFIRST_CHECK=CHECK_" + pfx;
        v += std::to_string(sz);
        args.push_back(v);
    }
    outputFile.close();
}

/*
 *
 */
int BBB48StringOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Init(JSON)\n");

    m_subType = config["subType"].asString();
    m_channelCount = config["channelCount"].asInt();
    m_gpio0Data.copyToPru = false;

    std::string verPostf = "";
    Json::Value root;
    if (!CapeUtils::INSTANCE.getStringConfig(m_subType, root)) {
        // might have the version number on it
        if (config["pinoutVersion"].asString() == "2.x") {
            verPostf = "-v2";
        }
        if (config["pinoutVersion"].asString() == "3.x") {
            verPostf = "-v3";
        }
        if (!CapeUtils::INSTANCE.getStringConfig(m_subType + verPostf, root)) {
            LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s%s\n", m_subType.c_str(), verPostf.c_str());
            WarningHolder::AddWarning("BBB48String: Could not read pin configuration for " + m_subType + verPostf);
            return 0;
        }
    }
    m_licensedOutputs = CapeUtils::INSTANCE.getLicensedOutputs();
    config["base"] = root;


    int maxStringLen = 0;
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s, &root["outputs"][i]))
            return 0;

        if (newString->m_outputChannels > maxStringLen) {
            maxStringLen = newString->m_outputChannels;
        }

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

    std::vector<std::string> split1args;
    split1args.push_back("-DRUNNING_ON_PRU" + std::to_string(BBB_PRU ? 1 : 0));
    std::vector<std::string> split0args;
    split0args.push_back("-DRUNNING_ON_PRU" + std::to_string(BBB_PRU ? 0 : 1));

    if (config.isMember("pixelTiming")) {
        int pixelTiming = config["pixelTiming"].asInt();
        if (pixelTiming) {
            args.push_back("-DPIXELTYPE_SLOW");
        }
    }

    std::vector<int> allStringMap;
    std::string outputList;
    int allMax = 0;
    for (int x = 0; x < m_strings.size(); x++) {
        if (m_strings[x]->m_outputChannels > 0) {
            //need to output this pin, configure it
            const PinCapabilities& pin = PinCapabilities::getPinByName(root["outputs"][x]["pin"].asString());
            pin.configPin();
            allStringMap.push_back(x);
            if (x >= m_licensedOutputs && m_strings[x]->m_outputChannels > 0) {
                // apply limit
                int pixels = 50;
                int chanCount = 0;
                for (auto& a : m_strings[x]->m_virtualStrings) {
                    if (pixels < a.pixelCount) {
                        a.pixelCount = pixels;
                        if (outputList != "") {
                            outputList += ", ";
                        }
                        outputList += std::to_string(x + 1);
                    }
                    pixels -= a.pixelCount;
                    chanCount += a.pixelCount * a.channelsPerNode();
                }
                if (m_strings[x]->m_isSmartReceiver) {
                    chanCount = 0;
                    if (outputList != "") {
                        outputList += ", ";
                    }
                    outputList += std::to_string(x + 1);
                }
                m_strings[x]->m_outputChannels = chanCount;
            }
            allMax = std::max(allMax, m_strings[x]->m_outputChannels);
            if (pin.gpioIdx == 0) {
                m_gpio0Data.gpioStringMap.push_back(x);
                m_gpio0Data.maxStringLen = std::max(m_gpio0Data.maxStringLen, m_strings[x]->m_outputChannels);
            } else {
                m_gpioData.gpioStringMap.push_back(x);
                m_gpioData.maxStringLen = std::max(m_gpioData.maxStringLen, m_strings[x]->m_outputChannels);
            }
        }
    }

    if (outputList != "") {
        std::string warning = "BBB48String is licensed for ";
        warning += std::to_string(m_licensedOutputs);
        warning += " outputs, but one or more outputs is configured for more than 50 pixels.  Output(s): ";
        warning += outputList;
        WarningHolder::AddWarning(warning);
        LogWarn(VB_CHANNELOUT, "WARNING: %s\n", warning.c_str());
    }

    if (m_gpioData.gpioStringMap.empty()) {
        m_gpioData.gpioStringMap = m_gpio0Data.gpioStringMap;
        m_gpioData.maxStringLen = m_gpio0Data.maxStringLen;
        m_gpio0Data.maxStringLen = 0;
        m_gpio0Data.gpioStringMap.clear();
    }
    bool hasSerial = true;
    if (config.isMember("serialInUse")) {
        hasSerial = config["serialInUse"].asBool();
    }

    if (!m_gpio0Data.maxStringLen) {
        //no GPIO0 output so no need for the second PRU to be used
        hasSerial = true;
    }
    bool canSplit = !hasSerial;
    if (hasSerial && m_gpio0Data.maxStringLen && ((m_gpio0Data.maxStringLen + m_gpioData.maxStringLen) < 2000)) {
        //if there is plenty of time to output the GPIO0 stuff
        //after the other GPIO's, let's do that
        args.push_back("-DSPLIT_GPIO0");
        args.push_back("-DGPIO0OUTPUTS=" + std::to_string(mapSize(48, m_gpio0Data.gpioStringMap.size())));

        m_gpio0Data.gpioStringMap.insert(m_gpio0Data.gpioStringMap.end(),
                                         m_gpioData.gpioStringMap.begin(),
                                         m_gpioData.gpioStringMap.end());
        m_gpioData.gpioStringMap = m_gpio0Data.gpioStringMap;
        m_gpio0Data.gpioStringMap.clear();
        m_gpioData.maxStringLen = std::max(m_gpioData.maxStringLen, m_gpio0Data.maxStringLen);
        m_gpio0Data.maxStringLen = 0;
        canSplit = false;
    }
    if (!canSplit) {
        m_gpioData.gpioStringMap = allStringMap;
        m_gpioData.maxStringLen = allMax;
        m_gpio0Data.gpioStringMap.clear();
    }

    int i = mapSize(root["outputs"].size(), m_gpioData.gpioStringMap.size());
    while (m_gpioData.gpioStringMap.size() < i) {
        m_gpioData.gpioStringMap.push_back(-1);
    }
    i = mapSize(root["outputs"].size(), m_gpio0Data.gpioStringMap.size());
    while (m_gpio0Data.gpioStringMap.size() < i) {
        m_gpio0Data.gpioStringMap.push_back(-1);
    }

    std::string v = "-DOUTPUTS=";
    v += std::to_string(m_gpioData.gpioStringMap.size());
    split1args.push_back(v);

    std::ofstream outputFile;
    outputFile.open("/tmp/PinConfiguration.hp", std::ofstream::out | std::ofstream::trunc);
    for (int x = 0; x < m_gpioData.gpioStringMap.size(); x++) {
        int idx = m_gpioData.gpioStringMap[x];
        if (idx >= 0) {
            const PinCapabilities& pin = PinCapabilities::getPinByName(root["outputs"][idx]["pin"].asString());
            outputFile << "#define o" << std::to_string(x + 1) << "_gpio  " << std::to_string(pin.gpioIdx) << "\n";
            outputFile << "#define o" << std::to_string(x + 1) << "_pin  " << std::to_string(pin.gpio) << "\n\n";
        } else {
            split1args.push_back("-DNOOUT" + std::to_string(x + 1));
        }
    }
    outputFile.close();
    if (m_gpio0Data.gpioStringMap.size() > 0) {
        split1args.push_back("-DDDRONLY");
        m_gpioData.copyToPru = false;
        m_gpio0Data.copyToPru = true;

        std::string v = "-DOUTPUTS=";
        v += std::to_string(m_gpio0Data.gpioStringMap.size());
        split0args.push_back(v);

        outputFile.open("/tmp/PinConfigurationGPIO0.hp", std::ofstream::out | std::ofstream::trunc);
        for (int x = 0; x < m_gpio0Data.gpioStringMap.size(); x++) {
            int idx = m_gpio0Data.gpioStringMap[x];
            if (idx >= 0) {
                const PinCapabilities& pin = PinCapabilities::getPinByName(root["outputs"][idx]["pin"].asString());
                outputFile << "#define o" << std::to_string(x + 1) << "_gpio  " << std::to_string(pin.gpioIdx) << "\n";
                outputFile << "#define o" << std::to_string(x + 1) << "_pin  " << std::to_string(pin.gpio) << "\n\n";
            } else {
                split0args.push_back("-DNOOUT" + std::to_string(x + 1));
            }
        }
        outputFile.close();
    }

#ifdef PRINT_STATS
    args.push_back("-DRECORD_STATS");
#endif

    createOutputLengths(m_gpioData, "", split1args);
    createOutputLengths(m_gpio0Data, "GPIO0_", split0args);

    //set the total data length (bytes, pixels * 3)
    v = "-DDATA_LEN=";
    v += std::to_string(m_gpioData.maxStringLen);
    split1args.push_back(v);
    v = "-DDATA_LEN=";
    v += std::to_string(m_gpio0Data.maxStringLen);
    split0args.push_back(v);

    compilePRUCode(args, split0args, split1args, !hasSerial);
    if (!StartPRU(!hasSerial)) {
        WarningHolder::AddWarning("BBB48String: Could not start PRU");
        return 0;
    }

    // give each area two chunks (frame flipping) of DDR memory
    m_gpioData.frameSize = m_gpioData.gpioStringMap.size() * m_gpioData.maxStringLen;
    m_gpioData.curData = m_pru->ddr;
    // leave a full memory page between to avoid conflicts
    int offset = ((m_gpioData.frameSize / 4096) + 2) * 4096;
    m_gpioData.lastData = m_gpioData.curData + offset;

    m_gpio0Data.frameSize = m_gpio0Data.gpioStringMap.size() * m_gpio0Data.maxStringLen;
    m_gpio0Data.curData = m_gpioData.lastData + offset;
    offset = ((m_gpio0Data.frameSize / 4096) + 2) * 4096;
    m_gpio0Data.lastData = m_gpio0Data.curData + offset;

    for (int x = 0; x < MAX_WS2811_TIMINGS; x++) {
        m_pruData->timings[x] = 0;
        if (m_pru0Data)
            m_pru0Data->timings[x] = 0;
    }
    PixelString::AutoCreateOverlayModels(m_strings);
    return retVal;
}

int BBB48StringOutput::StartPRU(bool both) {
    m_curFrame = 0;
    int pruNumber = BBB_PRU;

    m_pru = new BBBPru(pruNumber, true, true);
    m_pruData = (BBB48StringData*)m_pru->data_ram;
    m_pruData->command = 0;
    m_pruData->address_dma = m_pru->ddr_addr;
    m_pru->run("/tmp/FalconWS281x.out");

    if (both) {
        m_pru0 = new BBBPru(!pruNumber, true, true);
        m_pru0Data = (BBB48StringData*)m_pru0->data_ram;
        m_pru0Data->command = 0;
        m_pru0Data->address_dma = m_pru0->ddr_addr;
        m_pru0->run("/tmp/FalconWS281x_gpio0.out");
    }

    return 1;
}
void BBB48StringOutput::StopPRU(bool wait) {
    // Send the stop command
    m_pruData->response = 0;
    m_pruData->command = 0xFF;
    if (m_pru0Data) {
        m_pru0Data->response = 0;
        m_pru0Data->command = 0xFF;
    }
    __asm__ __volatile__("" ::
                             : "memory");

    int cnt = 0;
    while (wait && cnt < 25 && m_pruData->response != 0xFFFF) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cnt++;
        __asm__ __volatile__("" ::
                                 : "memory");
    }
    m_pru->stop();
    delete m_pru;

    if (m_pru0) {
        cnt = 0;
        while (wait && cnt < 25 && m_pru0Data->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
            __asm__ __volatile__("" ::
                                     : "memory");
        }
        m_pru0->stop();
        delete m_pru0;
    }
    m_pru = NULL;
    m_pru0 = NULL;
}
/*
 *
 */
int BBB48StringOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Close()\n");
    if (!m_gpioData.gpioStringMap.empty() || !m_gpio0Data.gpioStringMap.empty()) {
        StopPRU();
    }
    return ChannelOutput::Close();
}

void BBB48StringOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    PixelString* ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        int min = FPPD_MAX_CHANNELS;
        int max = -1;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
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
void BBB48StringOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType) {
    m_testCycle = cycleNum;
    m_testType = testType;
    m_testPercent = percentOfCycle;

    // We won't overlay the data here because we could have multiple strings
    // pointing at the same channel range so a per-port test cannot
    // be done via channel ranges.  We'll record the test information and use
    // that in prepData
}

void BBB48StringOutput::prepData(FrameData& d, unsigned char* channelData) {
    int count = 0;

    uint8_t* out = d.curData;

    PixelString* ps = NULL;
    uint8_t* c = NULL;

    int numStrings = d.gpioStringMap.size();
    for (int s = 0; s < numStrings; s++) {
        int idx = d.gpioStringMap[s];
        if (idx >= 0) {
            ps = m_strings[idx];
            c = out + s;

            bool output = true;
            if (m_testType && m_testCycle >= 0) {
                PixelStringTester* tester = PixelStringTester::getPixelStringTester(m_testType);
                if (tester) {
                    uint8_t* d = PixelStringTester::getPixelStringTester(m_testType)->createTestData(ps, m_testCycle, m_testPercent, channelData);
                    uint8_t* d2 = d;
                    for (int p = 0; p < ps->m_outputChannels; p++) {
                        *c = *d2;
                        c += numStrings;
                        ++d2;
                    }
                    delete[] d;
                    output = false;
                }
            }
            if (output) {
                for (int p = 0; p < ps->m_outputChannels; p++) {
                    int cn = ps->m_outputMap[p];
                    uint8_t* brightness = ps->m_brightnessMaps[p];
                    *c = brightness[channelData[cn]];
                    c += numStrings;
                }
            }
        }
    }
}

/*
 *
 */
void BBB48StringOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::PrepData(%p)\n", channelData);
    if (m_gpioData.gpioStringMap.empty() && m_gpio0Data.gpioStringMap.empty()) {
        return;
    }

    m_curFrame++;

#ifdef PRINT_STATS
    int max = 0;
    for (int x = 0; x < MAX_WS2811_TIMINGS; x++) {
        if (max < m_pruData->timings[x]) {
            max = m_pruData->timings[x];
        }
    }
    if (max > 300 || (m_curFrame % 10) == 1) {
        for (int x = 0; x < MAX_WS2811_TIMINGS;) {
            printf("%8X ", m_pruData->timings[x]);
            ++x;
            if ((x) % 16 == 0) {
                printf("\n");
            }
        }
        printf("\n%d:  max %d\n", m_curFrame, max);
    }
#endif
    prepData(m_gpioData, channelData);
    prepData(m_gpio0Data, channelData);
    m_testCycle = -1;
}

void BBB48StringOutput::sendData(FrameData& d, uintptr_t* dptr) {
    bool doSwap = false;
    if (d.copyToPru && (m_curFrame == 1 || memcmp(d.lastData, d.curData, std::min(d.frameSize, (uint32_t)24 * 1024)))) {
        //don't copy to PRU memory unless really needed to avoid bus contention

        //copy first 7.5K into PRU mem directly
        int fullsize = d.frameSize;
        int mx = d.frameSize;
        if (mx > (8 * 1024 - 512)) {
            mx = 8 * 1024 - 512;
        }

        //first 7.5K to main PRU ram
        memcpy(m_pru->data_ram + 512, d.curData, mx);
        fullsize -= 7628;
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (8 * 1024 - 512)) {
                outsize = 8 * 1024 - 512;
            }
            // second 7.5K to other PRU ram
            memcpy(m_pru->other_data_ram + 512, d.curData + 7628, outsize);
            fullsize -= 7628;
        }
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (12 * 1024)) {
                outsize = 12 * 1024;
            }
            memcpy(m_pru->shared_ram, d.curData + 7628 + 7628, outsize);
        }
    }

    // Map
    *dptr = (m_pru->ddr_addr + (d.curData - m_pru->ddr));
    std::swap(d.lastData, d.curData);
}

int BBB48StringOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::SendData(%p)\n", channelData);
    if (m_gpioData.gpioStringMap.empty() && m_gpio0Data.gpioStringMap.empty()) {
        return 0;
    }

    sendData(m_gpioData, &m_pruData->address_dma);
    sendData(m_gpio0Data, &m_pruData->address_dma_gpio0);
    if (m_pru0Data) {
        m_pru0Data->address_dma = m_pruData->address_dma_gpio0;
    }

    //make sure memory is flushed before command is set to 1
    __asm__ __volatile__("" ::
                             : "memory");

    // Send the start command
    m_pruData->command = 1;
    if (m_pru0Data)
        m_pru0Data->command = 1;

    return m_channelCount;
}

/*
 *
 */
void BBB48StringOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    type          : %s\n", m_subType.c_str());
    LogDebug(VB_CHANNELOUT, "    strings       : %d\n", m_strings.size());
    LogDebug(VB_CHANNELOUT, "    longest string: %d channels\n",
             std::max(m_gpio0Data.maxStringLen, m_gpioData.maxStringLen));

    for (int i = 0; i < m_strings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "    string #%02d\n", i);
        m_strings[i]->DumpConfig();
    }

    ChannelOutput::DumpConfig();
}
