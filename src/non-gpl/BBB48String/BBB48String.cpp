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

#include "fpp-json.h"

#include <sys/wait.h>
#include <unistd.h>

#include <arm_neon.h>

#include <algorithm>

#include <fstream>

#define BBB_PRU 1

// #define PRINT_STATS

// FPP includes
#include "../../Sequence.h"
#include "../FalconV5Support/FalconV5Support.h"
#include "../../Warnings.h"
#include "../../channeloutput/ChannelOutputSetup.h"
#include "../../common.h"
#include "../../log.h"

#include "BBB48String.h"
#include "../CapeUtils/CapeUtils.h"
#include "channeloutput/stringtesters/PixelStringTester.h"
#include "util/BBBUtils.h"

#include "../../overlays/PixelOverlay.h"

#include "Plugin.h"
class BBB48StringPlugin : public FPPPlugins::Plugin,
                          public FPPPlugins::ChannelOutputPlugin {
public:
    BBB48StringPlugin() : FPPPlugins::Plugin("BBB48String") {}
    virtual ChannelOutput*
    createChannelOutput(unsigned int startChannel,
                        unsigned int channelCount) override {
        return new BBB48StringOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() { return new BBB48StringPlugin(); }
}

/*
 *
 */
BBB48StringOutput::BBB48StringOutput(unsigned int startChannel,
                                     unsigned int channelCount) : ChannelOutput(startChannel, channelCount),
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
    for (auto a : m_strings) {
        delete a;
    }
    m_strings.clear();
    if (m_pru)
        delete m_pru;
    if (m_pru0)
        delete m_pru0;
    if (falconV5Support)
        delete falconV5Support;
}

static void compilePRUCode(const char* program,
                           const std::vector<std::string>& sargs,
                           const std::vector<std::string>& args1) {
    std::vector<std::string> args = sargs;
    args.insert(args.end(), args1.begin(), args1.end());
    CompilePRUProgram(program, args);
}

static void compilePRUCode(const std::vector<std::string>& sargs,
                           const std::vector<std::string>& args0,
                           const std::vector<std::string>& args1, bool split) {
    compilePRUCode("/opt/fpp/src/non-gpl/BBB48String/compileWS2811x.sh", sargs,
                   args1);
    if (split) {
        compilePRUCode("/opt/fpp/src/non-gpl/BBB48String/compileWS2811x_gpio0.sh",
                       sargs, args0);
    }
}

inline int mapSize(int max, int maxString) {
    if (maxString > max) {
        maxString = max;
    }
    int newHeight = maxString;
    // make sure it's a multiple of 4 to keep things aligned in memory
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

void BBB48StringOutput::createOutputLengths(FrameData& d,
                                            const std::string& pfx,
                                            std::vector<std::string>& args) {
    if (d.gpioStringMap.empty()) {
        return;
    }
    std::ofstream outputFile;
    outputFile.open("/tmp/OutputLengths" + pfx + ".hp",
                    std::ofstream::out | std::ofstream::trunc);

    std::map<int, std::vector<GPIOCommand>> sizes;
    for (int x : d.gpioStringMap) {
        if (x != -1) {
            int pc = m_strings[x]->m_outputBytes;
            if (pc != 0) {
                for (auto& a : m_strings[x]->m_gpioCommands) {
                    sizes[a.channelOffset].push_back(a);
                }
            }
        }
    }

    // Only offsets inside the frame get a check: the firmware turns every
    // line off when the frame ends, so a command at or past the longest
    // string never has anything to do.  The last key is not necessarily the
    // longest string's own end - a Falcon receiver chain has no end of
    // string command, and a license clamped string keeps commands past its
    // new length - so the chain is terminated explicitly rather than by
    // looking one key past the end of the map.
    std::vector<int> offsets;
    for (auto& s : sizes) {
        if (s.first < d.maxStringLen) {
            offsets.push_back(s.first);
        }
    }
    for (size_t k = 0; k < offsets.size(); k++) {
        int min = offsets[k];
        outputFile << "\nCHECK_" << pfx << std::to_string(min) << ":\n";
        if (min <= 255) {
            outputFile << "    QBNE skip_" << pfx << std::to_string(min)
                       << ", cur_data, " << std::to_string(min) << "\n";
        } else {
            if (min <= 0xFFFF) {
                outputFile << "    LDI r8, " << std::to_string(min) << "\n";
            } else {
                outputFile << "    LDI32 r8, " << std::to_string(min) << "\n";
            }
            outputFile << "    QBNE skip_" << pfx << std::to_string(min)
                       << ", cur_data, r8\n";
        }

        for (auto& cmd : sizes[min]) {
            int y = cmd.port;
            for (int x = 0; x < d.gpioStringMap.size(); x++) {
                if (d.gpioStringMap[x] == y) {
                    y = x;
                    break;
                }
            }

            std::string o = std::to_string(y + 1);
            if (cmd.type) {
                outputFile << "        SET GPIO_MASK(o" << o << "_gpio), GPIO_MASK(o"
                           << o << "_gpio), o" << o << "_pin\n";
            } else {
                outputFile << "        CLR GPIO_MASK(o" << o << "_gpio), GPIO_MASK(o"
                           << o << "_gpio), o" << o << "_pin\n";
            }
        }
        std::string next = "NO_PIXELS_CHECK";
        if (k + 1 < offsets.size()) {
            next = "CHECK_" + pfx + std::to_string(offsets[k + 1]);
        }
        outputFile << "        LDI next_check, $CODE(" << next << ")\n";
        outputFile << "skip_" << pfx << std::to_string(min) << ":\n        RET\n";
    }

    if (offsets.empty()) {
        args.emplace_back("-DFIRST_CHECK=NO_PIXELS_CHECK");
    } else {
        args.push_back("-DFIRST_CHECK=CHECK_" + pfx + std::to_string(offsets[0]));
    }
    outputFile.close();
}

void BBB48StringOutput::groupFalconV4Chains() {
    // Group Falcon smart receiver ports into 4-port chains.  Only the
    // send-only V4 protocol is possible here: these capes have no listener
    // hardware, so a V5 selection falls back to V4 handling.  Done before
    // the outputs are mapped so a chain whose first port has no pixels still
    // gets a lane to carry its receiver's config packet.
    int x = 0;
    while (x < m_strings.size()) {
        int p = x;
        PixelString* p1 = m_strings[x++];
        if (!p1->m_isSmartReceiver ||
            (p1->smartReceiverType != PixelString::ReceiverType::FalconV4 &&
             p1->smartReceiverType != PixelString::ReceiverType::FalconV5)) {
            continue;
        }
        if (p1->smartReceiverType == PixelString::ReceiverType::FalconV5) {
            LogWarn(VB_CHANNELOUT, "Falcon V5 (bidirectional) is not supported on BBB48String; using send-only V4 handling for output %d\n", p + 1);
            p1->smartReceiverType = PixelString::ReceiverType::FalconV4;
        }
        PixelString* p2 = x < m_strings.size() ? m_strings[x++] : nullptr;
        PixelString* p3 = x < m_strings.size() ? m_strings[x++] : nullptr;
        PixelString* p4 = x < m_strings.size() ? m_strings[x++] : nullptr;

        if (p1->m_isInverted) {
            AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: Falcon smart receivers are not supported on inverted output " + std::to_string(p + 1));
            continue;
        }
        if (!falconV5Support) {
            falconV5Support = new FalconV5Support();
        }
        falconV5Support->addReceiverChain(p1, p2, p3, p4, 0, 0, true);
        m_falconChainHeads.push_back(p);
        // the four lines carry continuous pixel data - none of the v1/v2
        // style mid-frame gpio toggling
        p1->m_gpioCommands.clear();
        if (p2) {
            p2->m_gpioCommands.clear();
        }
        if (p3) {
            p3->m_gpioCommands.clear();
        }
        if (p4) {
            p4->m_gpioCommands.clear();
        }
    }
}

void BBB48StringOutput::setupFalconV4Lanes(FrameData& d, const std::vector<std::array<uint8_t, 64>>& packets,
                                           Json::Value& root, std::vector<std::string>& args) {
    // The chain heads this PRU drives get their packet bytes in their lane
    // (bytes reversed: the firmware clocks MSB first, the wire wants the
    // UART LSB first) and their pin in the FALCON_MASK_GPIOn the firmware
    // toggles.  Every other lane stays zero and its line stays low.
    d.falconPacketLanes.clear();
    uint32_t masks[4] = { 0, 0, 0, 0 };
    int numStrings = d.gpioStringMap.size();
    std::vector<uint8_t> lanes(57 * numStrings, 0);
    for (int lane = 0; lane < numStrings; lane++) {
        int idx = d.gpioStringMap[lane];
        if (idx < 0 || std::find(m_falconChainHeads.begin(), m_falconChainHeads.end(), idx) == m_falconChainHeads.end()) {
            continue;
        }
        const PinCapabilities& pin = PinCapabilities::getPinByName(root["outputs"][idx]["pin"].asString());
        masks[pin.mappedGPIOIdx()] |= 1 << pin.mappedGPIO();
        for (int b = 0; b < 57; b++) {
            uint8_t v = packets[idx][b];
            v = ((v * 0x0802LU & 0x22110LU) | (v * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
            lanes[b * numStrings + lane] = v;
        }
    }
    if (!masks[0] && !masks[1] && !masks[2] && !masks[3]) {
        return;
    }
    d.falconPacketLanes = std::move(lanes);
    args.push_back("-DFALCONV4");
    for (int g = 0; g < 4; g++) {
        if (masks[g]) {
            char buf[40];
            snprintf(buf, sizeof(buf), "-DFALCON_MASK_GPIO%d=0x%08X", g, masks[g]);
            args.push_back(buf);
        }
    }
}

void BBB48StringOutput::setupFalconV4Support(Json::Value& root, std::vector<std::string>& args1, std::vector<std::string>& args0) {
    // The packet content is static for a configuration, so it is generated
    // once (after any license clamp has settled the pixel counts) and handed
    // to whichever PRU drives each chain head: GPIO0 heads may be on the
    // other PRU.
    if (!falconV5Support || falconV5Support->getReceiverChains().empty()) {
        return;
    }
    std::vector<std::array<uint8_t, 64>> packets;
    packets.resize(m_strings.size());
    for (auto& pk : packets) {
        memset(&pk[0], 0, 64);
    }
    for (auto& rc : falconV5Support->getReceiverChains()) {
        int port = rc->getPixelStrings()[0]->m_portNumber;
        if (port < 0 || port >= (int)packets.size()) {
            continue; // a port past the strings would write past the packets
        }
        rc->generateConfigPacket(&packets[port][0]);
    }
    setupFalconV4Lanes(m_gpioData, packets, root, args1);
    setupFalconV4Lanes(m_gpio0Data, packets, root, args0);
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
            LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s%s\n",
                   m_subType.c_str(), verPostf.c_str());
            AddOutputConfigWarning(
                13, "BBB48String: Could not read pin configuration for " + m_subType +
                        verPostf);
            return 0;
        }
    }

    m_licensedOutputs = CapeUtils::INSTANCE.getLicensedOutputs();

    // The mirror of the check in BBShiftString: this driver drives a header
    // pin per output, so every output the cape declares has to name one.  A
    // shift-register cape's pinout gives a shift stage instead and its
    // integer "pin" would come back through asString() as a pin name that
    // matches nothing, leaving every port silently unconfigured.
    const int capeOutputCount = root["outputs"].size();
    for (int i = 0; i < capeOutputCount; i++) {
        if (!root["outputs"][i]["pin"].isIntegral()) {
            continue;
        }
        std::string capeDriver = root.get("driver", "BBShiftString").asString();
        LogErr(VB_CHANNELOUT, "Cape %s is not a BBB48String cape - its string configuration gives shift register stages, so it needs the %s output type\n",
               m_subType.c_str(), capeDriver.c_str());
        AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: " + m_subType + " needs the " + capeDriver +
                                  " output type - open the Pixel Strings page and save to correct it");
        return 0;
    }

    config["base"] = root;

    int maxStringLen = 0;
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s, &root["outputs"][i]))
            return 0;

        // This PRU drives the pins directly and has no per-port idle level, so
        // an inverted protocol would put inverted data on a line that still
        // idles low - worse than not supporting it.  Undo the inversion and
        // drive the port as a normal ws281x string.  The preamble has to go
        // with it: those bytes are meaningless to a ws281x part and would
        // offset every pixel on the port.  (Cape declared inversion never
        // reaches here: no cape using this driver sets it.)
        // The fill loop here emits one byte per channel, so a 16 bit part
        // would come out at half length with the wrong bytes.  Refuse it and
        // drive the port 8 bit rather than produce garbage.
        if (newString->m_bytesPerChannel != 1) {
            LogWarn(VB_CHANNELOUT, "16 bit protocols are not supported on this cape; output %d will be driven as 8 bit\n", i + 1);
            AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: 16 bit protocols are not supported on this cape, output " + std::to_string(i + 1));
            int pre = (int)newString->m_preamble.size();
            newString->m_outputBytes = (newString->m_outputBytes - pre) / newString->m_bytesPerChannel + pre;
            newString->m_bytesPerChannel = 1;
            for (auto& vs : newString->m_virtualStrings) {
                vs.brightnessMap16 = nullptr;
            }
        }

        if (newString->m_protocolInverted) {
            LogWarn(VB_CHANNELOUT, "Inverted protocols are not supported on this cape; output %d will be driven as ws2811\n", i + 1);
            AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: inverted protocols are not supported on this cape, output " + std::to_string(i + 1));
            newString->invertOutput();
            newString->dropPreamble();
            newString->m_protocolInverted = false;
        }

        if (newString->m_outputBytes > maxStringLen) {
            maxStringLen = newString->m_outputBytes;
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

    groupFalconV4Chains();

    std::vector<int> allStringMap;
    std::string outputList;
    int allMax = 0;
    for (int x = 0; x < m_strings.size(); x++) {
        bool chainHead = std::find(m_falconChainHeads.begin(), m_falconChainHeads.end(), x) != m_falconChainHeads.end();
        if (m_strings[x]->m_outputBytes > 0 || chainHead) {
            if (x >= capeOutputCount) {
                LogErr(VB_CHANNELOUT, "Output %d is past the %d the %s cape declares\n",
                       x + 1, capeOutputCount, m_subType.c_str());
                AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: output " + std::to_string(x + 1) +
                                          " is past the end of the cape's pinout");
                continue;
            }
            // need to output this pin, configure it
            std::string pinName = root["outputs"][x]["pin"].asString();
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            m_usedGPIOS.push_back(pinName);
            pin.configPin("gpio", true, "BBB48String-" + std::to_string(x + 1));
            allStringMap.push_back(x);
            if (x >= m_licensedOutputs && m_strings[x]->m_outputBytes > 0) {
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
                m_strings[x]->setPixelDataChannels(chanCount);
            }
            allMax = std::max(allMax, m_strings[x]->m_outputBytes);
            if (pin.mappedGPIOIdx() == 0) {
                m_gpio0Data.gpioStringMap.push_back(x);
                m_gpio0Data.maxStringLen =
                    std::max(m_gpio0Data.maxStringLen, m_strings[x]->m_outputBytes);
            } else {
                m_gpioData.gpioStringMap.push_back(x);
                m_gpioData.maxStringLen =
                    std::max(m_gpioData.maxStringLen, m_strings[x]->m_outputBytes);
            }
        }
    }

    if (outputList != "") {
        std::string warning = "BBB48String is licensed for ";
        warning += std::to_string(m_licensedOutputs);
        warning += " outputs, but one or more outputs is configured for more than "
                   "50 pixels.  Output(s): ";
        warning += outputList;
        AddOutputConfigWarning(UNKNOWN_WARNING_ID, warning);
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
        // no GPIO0 output so no need for the second PRU to be used
        hasSerial = true;
    }
    bool canSplit = !hasSerial;
    if (hasSerial && m_gpio0Data.maxStringLen &&
        ((m_gpio0Data.maxStringLen + m_gpioData.maxStringLen) < 2000)) {
        // if there is plenty of time to output the GPIO0 stuff
        // after the other GPIO's, let's do that
        args.push_back("-DSPLIT_GPIO0");
        args.push_back(
            "-DGPIO0OUTPUTS=" +
            std::to_string(mapSize(48, m_gpio0Data.gpioStringMap.size())));

        m_gpio0Data.gpioStringMap.insert(m_gpio0Data.gpioStringMap.end(),
                                         m_gpioData.gpioStringMap.begin(),
                                         m_gpioData.gpioStringMap.end());
        m_gpioData.gpioStringMap = m_gpio0Data.gpioStringMap;
        m_gpio0Data.gpioStringMap.clear();
        m_gpioData.maxStringLen =
            std::max(m_gpioData.maxStringLen, m_gpio0Data.maxStringLen);
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
    // The firmware loads one byte per output into r10-r21; past 48 the load
    // runs on into the GPIO bank address registers and every later GPIO
    // store goes to an address made of pixel data.
    if (m_gpioData.gpioStringMap.size() > 48 || m_gpio0Data.gpioStringMap.size() > 48) {
        LogErr(VB_CHANNELOUT, "%s uses %d outputs on one PRU, more than the 48 the firmware supports\n",
               m_subType.c_str(), (int)std::max(m_gpioData.gpioStringMap.size(), m_gpio0Data.gpioStringMap.size()));
        AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: " + m_subType + " uses more than 48 outputs on one PRU");
        return 0;
    }

    setupFalconV4Support(root, split1args, split0args);

    std::string v = "-DOUTPUTS=";
    v += std::to_string(m_gpioData.gpioStringMap.size());
    split1args.push_back(v);

    std::ofstream outputFile;
    outputFile.open("/tmp/PinConfiguration.hp",
                    std::ofstream::out | std::ofstream::trunc);
    for (int x = 0; x < m_gpioData.gpioStringMap.size(); x++) {
        int idx = m_gpioData.gpioStringMap[x];
        if (idx >= 0) {
            const PinCapabilities& pin =
                PinCapabilities::getPinByName(root["outputs"][idx]["pin"].asString());
            outputFile << "#define o" << std::to_string(x + 1) << "_gpio  "
                       << std::to_string(pin.mappedGPIOIdx()) << "\n";
            outputFile << "#define o" << std::to_string(x + 1) << "_pin  "
                       << std::to_string(pin.mappedGPIO()) << "\n\n";
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

        outputFile.open("/tmp/PinConfigurationGPIO0.hp",
                        std::ofstream::out | std::ofstream::trunc);
        for (int x = 0; x < m_gpio0Data.gpioStringMap.size(); x++) {
            int idx = m_gpio0Data.gpioStringMap[x];
            if (idx >= 0) {
                const PinCapabilities& pin = PinCapabilities::getPinByName(
                    root["outputs"][idx]["pin"].asString());
                outputFile << "#define o" << std::to_string(x + 1) << "_gpio  "
                           << std::to_string(pin.mappedGPIOIdx()) << "\n";
                outputFile << "#define o" << std::to_string(x + 1) << "_pin  "
                           << std::to_string(pin.mappedGPIO()) << "\n\n";
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

    compilePRUCode(args, split0args, split1args, !hasSerial);
    if (!StartPRU(!hasSerial)) {
        AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: Could not start PRU");
        return 0;
    }

    // give each area two chunks (frame flipping) of DDR memory, from the
    // shared region allocator so other outputs on the region (BBBMatrix
    // panels, BBBSerial - e.g. the K8-B-Scroller runs all three) cannot
    // overlap us
    // each with room for the Falcon V4 config packet appended after the
    // frame data
    m_gpioData.frameSize =
        m_gpioData.gpioStringMap.size() * std::max(2400, m_gpioData.maxStringLen);
    m_gpioData.frameSize += m_gpioData.falconPacketLanes.size();
    // leave a full memory page between to avoid conflicts
    int offset = ((m_gpioData.frameSize / 4096) + 2) * 4096;
    m_gpio0Data.frameSize = m_gpio0Data.gpioStringMap.size() *
                            std::max(2400, m_gpio0Data.maxStringLen);
    m_gpio0Data.frameSize += m_gpio0Data.falconPacketLanes.size();
    int offset0 = ((m_gpio0Data.frameSize / 4096) + 2) * 4096;
    uint32_t ddrPhys = 0;
    uint8_t* ddrBase = BBBPru::ddrAlloc("BBB48String", 2 * offset + 2 * offset0, ddrPhys);
    if (!ddrBase) {
        LogErr(VB_CHANNELOUT, "BBB48String: no room in the PRU DDR region\n");
        AddOutputConfigWarning(20, "BBB48String: no room in the PRU DDR region - reduce string lengths or other outputs' buffers");
        return 0;
    }
    m_gpioData.curData = ddrBase;
    m_gpioData.lastData = m_gpioData.curData + offset;

    m_gpio0Data.curData = m_gpioData.lastData + offset;
    m_gpio0Data.lastData = m_gpio0Data.curData + offset0;

    // flag the virtual strings whose channel map is a plain run so prepData
    // can walk the channel data directly instead of through the map
    m_vsAffine.resize(m_strings.size());
    for (size_t s = 0; s < m_strings.size(); s++) {
        m_vsAffine[s].resize(m_strings[s]->m_virtualStrings.size());
        for (size_t v = 0; v < m_strings[s]->m_virtualStrings.size(); v++) {
            auto& vs = m_strings[s]->m_virtualStrings[v];
            bool affine = true;
            for (int i = 0; i + 3 < vs.chMapCount; i++) {
                if (vs.chMap[i + 3] != vs.chMap[i] + 3) {
                    affine = false;
                    break;
                }
            }
            m_vsAffine[s][v] = affine;
        }
    }

    PixelString::AutoCreateOverlayModels(m_strings, m_autoCreatedModelNames);
    return retVal;
}

int BBB48StringOutput::StartPRU(bool both) {
    m_curFrame = 0;
    int pruNumber = BBB_PRU;

    // The firmware is assembled per configuration just before this, so a
    // missing program is a build failure (the assembler's errors are in the
    // fppd output), not a PRU that needs a reboot.  Like the others here it
    // is a config warning: it comes down when the outputs are next loaded.
    std::vector<std::string> programs = { "/tmp/FalconWS281x.out" };
    if (both) {
        programs.push_back("/tmp/FalconWS281x_gpio0.out");
    }
    for (auto& fw : programs) {
        if (!FileExists(fw)) {
            LogErr(VB_CHANNELOUT, "BBB48String: PRU firmware %s failed to build\n", fw.c_str());
            AddOutputConfigWarning(UNKNOWN_WARNING_ID, "BBB48String: PRU firmware failed to build for this configuration");
            return 0;
        }
    }
    // run() is told not to clear PRU memory.  The firmware zeroes its own
    // control block (address, command, response) before it first reads the
    // command, and reads nothing else until a non-zero command arrives.
    // SendData() writes that command only after the frame's address, and on
    // the first frame a full copy of its pixel data into the PRU memories, so
    // a stale word left from an earlier run can neither start output nor be
    // sent as pixel data.  Skipping the clear also keeps this from touching
    // the PRUSS while both cores are offline (crash e80ad9caf7).
    m_pru = new BBBPru(pruNumber, true, true);
    m_pruData = (BBB48StringData*)m_pru->data_ram;
    if (!m_pru->run("/tmp/FalconWS281x.out", true, false)) {
        LogErr(VB_CHANNELOUT,
               "BBB48String: Unable to start PRU. May require a reboot.\n");
        AddOutputConfigWarning(UNKNOWN_WARNING_ID,
            "BBB48String: Unable to start PRU. May require a reboot.");
        return 0;
    }
    m_pruData->command = 0;
    m_pruData->address_dma = m_pru->ddr_addr;

    if (both) {
        m_pru0 = new BBBPru(!pruNumber, true, true);
        m_pru0Data = (BBB48StringData*)m_pru0->data_ram;
        if (!m_pru0->run("/tmp/FalconWS281x_gpio0.out", true, false)) {
            LogErr(VB_CHANNELOUT,
                   "BBB48String: Unable to start PRU0. May require a reboot.\n");
            AddOutputConfigWarning(UNKNOWN_WARNING_ID,
                "BBB48String: Unable to start PRU0. May require a reboot.");
            return 0;
        }
        m_pru0Data->command = 0;
        m_pru0Data->address_dma = m_pru0->ddr_addr;
    }

    return 1;
}
void BBB48StringOutput::StopPRU(bool wait) {
    // Send the stop command
    m_pruData->response = 0;
    m_pruData->command = 0xFFFF;
    if (m_pru0Data) {
        m_pru0Data->response = 0;
        m_pru0Data->command = 0xFFFF;
    }
    __asm__ __volatile__("" ::: "memory");

    // Wait for BOTH cores to acknowledge before stopping EITHER. Stopping a
    // core through remoteproc can leave the PRUSS data RAM unreachable, and a
    // read of it then takes an external abort (SIGBUS) rather than returning
    // a value - which is what happened when PRU0's response was polled after
    // PRU1 had already been stopped. So every access to PRU RAM here finishes
    // while both cores are still up, and nothing touches it afterwards.
    int cnt = 0;
    while (wait && cnt < 25 && m_pruData->response != 0xFFFF) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cnt++;
        __asm__ __volatile__("" ::: "memory");
    }
    if (m_pru0) {
        cnt = 0;
        while (wait && cnt < 25 && m_pru0Data->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
            __asm__ __volatile__("" ::: "memory");
        }
    }

    m_pru->stop();
    delete m_pru;
    if (m_pru0) {
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
    BBBPru::ddrRelease("BBB48String");
    for (auto& n : m_autoCreatedModelNames) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(n);
    }
    if (!m_gpioData.gpioStringMap.empty() || !m_gpio0Data.gpioStringMap.empty()) {
        StopPRU();
    }
    return ChannelOutput::Close();
}

void BBB48StringOutput::GetRequiredChannelRanges(
    const std::function<void(int, int)>& addRange) {
    PixelString* ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        int min = FPPD_MAX_CHANNELS;
        int max = -1;
        for (int p = 0; p < ps->m_outputBytes; p++) {
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
void BBB48StringOutput::OverlayTestData(unsigned char* channelData,
                                        int cycleNum, float percentOfCycle,
                                        int testType,
                                        const Json::Value& config) {
    m_testCycle = cycleNum;
    m_testType = testType;
    m_testPercent = percentOfCycle;

    // We won't overlay the data here because we could have multiple strings
    // pointing at the same channel range so a per-port test cannot
    // be done via channel ranges.  We'll record the test information and use
    // that in prepData
}

// 8x8 byte transpose: in, rows are 8 consecutive bytes of 8 strings; out,
// rows are the 8 strings' bytes for 8 consecutive positions.  (Same helper
// as BBShiftString.)
static inline void transpose8x8(uint8x8_t r[8]) {
    uint8x8x2_t t0 = vtrn_u8(r[0], r[1]);
    uint8x8x2_t t1 = vtrn_u8(r[2], r[3]);
    uint8x8x2_t t2 = vtrn_u8(r[4], r[5]);
    uint8x8x2_t t3 = vtrn_u8(r[6], r[7]);
    uint16x4x2_t s0 = vtrn_u16(vreinterpret_u16_u8(t0.val[0]), vreinterpret_u16_u8(t1.val[0]));
    uint16x4x2_t s1 = vtrn_u16(vreinterpret_u16_u8(t0.val[1]), vreinterpret_u16_u8(t1.val[1]));
    uint16x4x2_t s2 = vtrn_u16(vreinterpret_u16_u8(t2.val[0]), vreinterpret_u16_u8(t3.val[0]));
    uint16x4x2_t s3 = vtrn_u16(vreinterpret_u16_u8(t2.val[1]), vreinterpret_u16_u8(t3.val[1]));
    uint32x2x2_t u0 = vtrn_u32(vreinterpret_u32_u16(s0.val[0]), vreinterpret_u32_u16(s2.val[0]));
    uint32x2x2_t u1 = vtrn_u32(vreinterpret_u32_u16(s1.val[0]), vreinterpret_u32_u16(s3.val[0]));
    uint32x2x2_t u2 = vtrn_u32(vreinterpret_u32_u16(s0.val[1]), vreinterpret_u32_u16(s2.val[1]));
    uint32x2x2_t u3 = vtrn_u32(vreinterpret_u32_u16(s1.val[1]), vreinterpret_u32_u16(s3.val[1]));
    r[0] = vreinterpret_u8_u32(u0.val[0]);
    r[1] = vreinterpret_u8_u32(u1.val[0]);
    r[2] = vreinterpret_u8_u32(u2.val[0]);
    r[3] = vreinterpret_u8_u32(u3.val[0]);
    r[4] = vreinterpret_u8_u32(u0.val[1]);
    r[5] = vreinterpret_u8_u32(u1.val[1]);
    r[6] = vreinterpret_u8_u32(u2.val[1]);
    r[7] = vreinterpret_u8_u32(u3.val[1]);
}

void BBB48StringOutput::prepData(FrameData& d, unsigned char* channelData) {
    PixelStringTester* tester = nullptr;
    if (m_testType && m_testCycle >= 0) {
        tester = PixelStringTester::getPixelStringTester(m_testType);
        // null for a test type no string tester implements; the strings then
        // just send their normal data
        if (tester) {
            tester->prepareTestData(m_testCycle, m_testPercent);
        }
    }
    const int numStrings = d.gpioStringMap.size();
    // per string slot: either a fully prepared buffer (test mode) or the
    // PixelString whose virtual strings are rendered on the fly per tile,
    // with a cursor tracking where in the virtual string list the next tile
    // continues.  (Same scheme as BBShiftString::prepData.)
    struct SlotSrc {
        const uint8_t* buf = nullptr;
        PixelString* ps = nullptr;
        const uint8_t* affine = nullptr;
        uint32_t len = 0;
        uint32_t vsIdx = 0;
        uint32_t vsOff = 0;
    };
    std::vector<SlotSrc> slots(numStrings);
    uint32_t newMaxLen = 0;
    for (int s = 0; s < numStrings; s++) {
        int idx = d.gpioStringMap[s];
        if (idx >= 0) {
            PixelString* ps = m_strings[idx];
            uint32_t newLen = ps->m_outputBytes;
            SlotSrc& sl = slots[s];
            if (tester) {
                sl.buf = tester->createTestData(ps, m_testCycle, m_testPercent,
                                                channelData, newLen);
            } else {
                sl.ps = ps;
                sl.affine = m_vsAffine[idx].data();
            }
            sl.len = newLen;
            newMaxLen = std::max(newLen, newMaxLen);
        }
    }
    d.outputStringLen = newMaxLen;

    // Interleave the string data in tiles that stay in the L1 cache instead
    // of writing a byte every numStrings bytes across the whole frame, with
    // the brightness/gamma application fused into the tile fill.  Virtual
    // strings whose channel map is a simple run skip the per-channel map
    // indirection.  Unmapped slots and the tail of strings shorter than the
    // longest now output zeros instead of stale buffer content.
    uint8_t* out = d.curData;
    constexpr uint32_t TILE = 64;
    uint8_t col[8][TILE];
    for (uint32_t p0 = 0; p0 < newMaxLen; p0 += TILE) {
        const uint32_t n = std::min(TILE, newMaxLen - p0);
        const uint32_t nFull = n & ~7;
        for (int s0 = 0; s0 < numStrings; s0 += 8) {
            const int ns = std::min(8, numStrings - s0);
            for (int x = 0; x < ns; ++x) {
                SlotSrc& sl = slots[s0 + x];
                uint32_t avail = sl.len > p0 ? std::min(n, sl.len - p0) : 0;
                uint32_t p = 0;
                if (sl.buf) {
                    memcpy(col[x], sl.buf + p0, avail);
                    p = avail;
                } else if (sl.ps) {
                    auto& vstrings = sl.ps->m_virtualStrings;
                    while (p < avail && sl.vsIdx < vstrings.size()) {
                        auto& vs = vstrings[sl.vsIdx];
                        uint32_t m = std::min(avail - p, (uint32_t)vs.chMapCount - sl.vsOff);
                        const uint8_t* br = vs.brightnessMap;
                        const int* mp = vs.chMap;
                        if (sl.affine[sl.vsIdx]) {
                            uint32_t c = sl.vsOff % 3;
                            int32_t base = (int32_t)(sl.vsOff - c);
                            for (uint32_t k = 0; k < m; ++k) {
                                col[x][p++] = br[channelData[mp[c] + base]];
                                if (++c == 3) {
                                    c = 0;
                                    base += 3;
                                }
                            }
                        } else {
                            const int* mo = mp + sl.vsOff;
                            for (uint32_t k = 0; k < m; ++k) {
                                col[x][p++] = br[channelData[mo[k]]];
                            }
                        }
                        sl.vsOff += m;
                        if (sl.vsOff >= (uint32_t)vs.chMapCount) {
                            sl.vsOff = 0;
                            sl.vsIdx++;
                        }
                    }
                }
                if (p < n) {
                    memset(&col[x][p], 0, n - p);
                }
            }
            uint32_t g = 0;
            if (ns == 8) {
                for (; g < nFull; g += 8) {
                    uint8x8_t r[8];
                    for (int x = 0; x < 8; ++x) {
                        r[x] = vld1_u8(&col[x][g]);
                    }
                    transpose8x8(r);
                    for (int i = 0; i < 8; i++) {
                        vst1_u8(&out[(size_t)(p0 + g + i) * numStrings + s0], r[i]);
                    }
                }
            }
            for (uint32_t p = g; p < n; ++p) {
                for (int x = 0; x < ns; ++x) {
                    out[(size_t)(p0 + p) * numStrings + s0 + x] = col[x][p];
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

    if (!m_falconChainHeads.empty() && (m_curFrame % 12) == 1) {
        // append the config packet after the frame data; the firmware sends
        // it when the command has the packet bit set
        for (FrameData* d : { &m_gpioData, &m_gpio0Data }) {
            if (!d->falconPacketLanes.empty()) {
                memcpy(d->curData + d->outputStringLen * d->gpioStringMap.size(),
                       d->falconPacketLanes.data(), d->falconPacketLanes.size());
            }
        }
        m_sendFalconPacket = true;
    }
}

void BBB48StringOutput::sendData(FrameData& d, uint32_t* dptr) {
    bool doSwap = false;
    // compare everything the copy below would place in PRU memory - the
    // firmware reads those bytes from there rather than DDR, so a change
    // past the compared range would leave it sending the old data
    uint32_t pruCopySize = std::min(d.frameSize, (uint32_t)(7624 + 7624 + m_pru->shared_ram_size));
    if (d.copyToPru &&
        (m_curFrame == 1 || memcmp(d.lastData, d.curData, pruCopySize))) {
        // don't copy to PRU memory unless really needed to avoid bus contention

        // copy first 7.5K into PRU mem directly
        int fullsize = d.frameSize;
        int mx = d.frameSize;
        if (mx > (8 * 1024 - 512)) {
            mx = 8 * 1024 - 512;
        }

        // first 7.5K to main PRU ram
        m_pru->memcpyToPRU(m_pru->data_ram + 512, d.curData, mx);
        fullsize -= 7624;
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (8 * 1024 - 512)) {
                outsize = 8 * 1024 - 512;
            }
            // second 7.5K to other PRU ram
            m_pru->memcpyToPRU(m_pru->other_data_ram + 512, d.curData + 7624,
                               outsize);
            fullsize -= 7624;
        }
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > m_pru->shared_ram_size) {
                outsize = m_pru->shared_ram_size;
            }
            m_pru->memcpyToPRU(m_pru->shared_ram, d.curData + 7624 + 7624, outsize);
        }
    }

    // Map
    size_t offset = d.curData - m_pru->ddr;
    *dptr = (m_pru->ddr_addr + offset);
    std::swap(d.lastData, d.curData);
}

int BBB48StringOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::SendData(%p)\n", channelData);
    if (m_gpioData.gpioStringMap.empty() && m_gpio0Data.gpioStringMap.empty()) {
        return 0;
    }

    sendData(m_gpioData, &m_pruData->address_dma);
    if (m_pru0Data) {
        sendData(m_gpio0Data, &m_pru0Data->address_dma);
    }

    // make sure memory is flushed before command is set to 1
    __asm__ __volatile__("" ::: "memory");

    // Send the start command
    uint32_t flag = 0;
    if (m_testType == 999) {
        // pixel counting needs the GPIO commands and such turned off
        flag = 0x8000;
    }
    // bit 16: a Falcon V4 config packet follows the frame data, for the
    // PRUs that drive a chain head
    uint32_t flag0 = flag;
    if (m_sendFalconPacket) {
        if (!m_gpioData.falconPacketLanes.empty()) {
            flag |= 0x10000;
        }
        if (!m_gpio0Data.falconPacketLanes.empty()) {
            flag0 |= 0x10000;
        }
        m_sendFalconPacket = false;
    }
    if (m_pru0Data) {
        m_pru0Data->command = m_gpio0Data.outputStringLen | flag0;
    }
    m_pruData->command = m_gpioData.outputStringLen | flag;

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
