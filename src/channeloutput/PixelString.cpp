/*
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

#include "../Sequence.h"
#include "../log.h"

#include "PixelString.h"
#include "../OutputMonitor.h"
#include "../Warnings.h"
#include "overlays/PixelOverlay.h"

/////////////////////////////////////////////////////////////////////////////

#define MAX_PIXEL_STRING_LENGTH 1600

constexpr uint32_t SMART_RECEIVER_LEADIN = 6 * 3;
constexpr uint32_t SMART_RECEIVER_LEN = 6 * 3;
constexpr uint32_t SMART_RECEIVER_LEADOUT = 6 * 3;

constexpr uint32_t SMART_RECEIVER_V2_GAP = 10;

#define CHECKPS_SETTING(SETTING)                                                           \
    if (SETTING) {                                                                         \
        LogErr(VB_CHANNELOUT, "Invalid PixelString Config %s\n", #SETTING);                \
        WarningHolder::AddWarning("Invalid PixelString Config: " + std::string(#SETTING)); \
        return 0;                                                                          \
    }

const char* SMART_RECEIVER_LABELS[] = {
    "virtualStrings",
    "virtualStringsB",
    "virtualStringsC",
    "virtualStringsD",
    "virtualStringsE",
    "virtualStringsF",
};

VirtualString::VirtualString() :
    whiteOffset(-1),
    receiverNum(-1),
    startChannel(0),
    pixelCount(0),
    groupCount(0),
    reverse(0),
    colorOrder(FPPColorOrder::kColorOrderRGB),
    startNulls(0),
    endNulls(0),
    zigZag(0),
    brightness(100),
    gamma(1.0),
    leadInCount(0),
    toggleCount(0),
    leadOutCount(0) {
}

VirtualString::VirtualString(int rt, int rn) :
    whiteOffset(-1),
    receiverNum(rn),
    startChannel(0),
    pixelCount(0),
    groupCount(0),
    reverse(0),
    colorOrder(FPPColorOrder::kColorOrderRGB),
    startNulls(0),
    endNulls(0),
    zigZag(0),
    brightness(100),
    gamma(1.0),
    leadInCount(0),
    toggleCount(0),
    leadOutCount(0) {
    switch (rt) {
    case 0:
        leadInCount = 0;
        toggleCount = SMART_RECEIVER_LEN;
        leadOutCount = SMART_RECEIVER_LEADOUT;
        break;
    case 1:
        leadInCount = SMART_RECEIVER_LEADIN;
        toggleCount = SMART_RECEIVER_LEN * 2;
        leadOutCount = SMART_RECEIVER_LEADOUT;
        break;
    case 2:
        leadInCount = SMART_RECEIVER_LEADIN;
        toggleCount = SMART_RECEIVER_LEN * 3;
        leadOutCount = SMART_RECEIVER_LEADOUT;
        break;
    case 3:
        leadInCount = SMART_RECEIVER_V2_GAP;
        toggleCount = 0;
        leadOutCount = 0;
        break;
    case 4: // Falcon v5
        leadInCount = 0;
        toggleCount = 0;
        leadOutCount = 0;
        break;
    }
    pixelCount = leadInCount + toggleCount + leadOutCount;
    for (int x = 0; x < 256; x++) {
        brightnessMap[x] = x;
    }
}
bool VirtualString::isSmartReceiver() const {
    return receiverNum >= 0;
}
bool VirtualString::isFalconV5SmartReceiver() const {
    return receiverNum >= 0 && leadInCount == 0 && toggleCount == 0 && leadOutCount == 0;
}

int VirtualString::channelsPerNode() const {
    if (receiverNum >= 0) {
        return 1;
    }
    if (colorOrder == FPPColorOrder::kColorOrderONE) {
        return 1;
    }
    return whiteOffset == -1 ? 3 : 4;
}

/*
 *
 */
PixelString::PixelString(bool supportSmart) :
    m_portNumber(0),
    m_channelOffset(0),
    m_outputChannels(0),
    m_isSmartReceiver(supportSmart),
    m_brightnessMaps(nullptr),
    m_outputBuffer(nullptr) {
}

/*
 *
 */
PixelString::~PixelString() {
    if (m_brightnessMaps) {
        free(m_brightnessMaps);
    }
    if (m_outputBuffer) {
        free(m_outputBuffer);
    }
}

/*
 *
 */
int PixelString::Init(Json::Value config, Json::Value* pinConfig) {
    m_portNumber = config["portNumber"].asInt();
    m_outputChannels = 0;

    smartReceiverType = ReceiverType::Standard;
    int receiverCount = 1;
    if (m_isSmartReceiver && config.isMember("differentialType")) {
        int rt = config["differentialType"].asInt();
        if (rt) {
            if (rt <= 3) {
                // v1 smart receivers
                receiverCount = rt;
                smartReceiverType = ReceiverType::v1;
            } else if (rt > 9) {
                // Falcon V5 smart receivers
                receiverCount = rt - 9;
                smartReceiverType = ReceiverType::FalconV5;
            } else {
                // v2 smart receivers
                receiverCount = rt - 3;
                smartReceiverType = ReceiverType::v2;
            }
        }
    }
    m_isSmartReceiver = smartReceiverType != ReceiverType::Standard;
    if (smartReceiverType == ReceiverType::v1) {
        // v1 needs a lead in
        AddVirtualString(VirtualString(0, 0));
    }
    int startMaxChan = m_outputChannels;
    for (int i = 0; i < config["virtualStrings"].size(); i++) {
        Json::Value vsc = config["virtualStrings"][i];
        VirtualString vs;
        if (!ReadVirtualString(vsc, vs)) {
            return 0;
        }
        AddVirtualString(vs);
    }
    std::array<bool, 6> hasChannels;
    hasChannels[0] = startMaxChan != m_outputChannels;
    if (!hasChannels[0] && (smartReceiverType == ReceiverType::v1 || smartReceiverType == ReceiverType::v2)) {
        // we need to output at least 1 pixel
        AddNullPixelString();
    }
    for (int p = 1; p < receiverCount; p++) {
        int sz = m_virtualStrings.size();
        int mc = m_outputChannels;

        if (smartReceiverType == ReceiverType::v1) {
            // v1 smart  receiver
            AddVirtualString(VirtualString(p, p));
        } else if (smartReceiverType == ReceiverType::v2) {
            // v2 smart  receiver, .15ms gap
            AddVirtualString(VirtualString(3, p));
        } else if (smartReceiverType == ReceiverType::FalconV5) {
            // Falcon v5, just marker to determine config packets later
            AddVirtualString(VirtualString(4, p));
        }
        startMaxChan = m_outputChannels;
        for (int i = 0; i < config[SMART_RECEIVER_LABELS[p]].size(); i++) {
            Json::Value vsc = config[SMART_RECEIVER_LABELS[p]][i];
            VirtualString vs;
            if (!ReadVirtualString(vsc, vs)) {
                return 0;
            }
            AddVirtualString(vs);
        }
        hasChannels[p] = startMaxChan != m_outputChannels;
        hasChannels[0] |= hasChannels[p];
        if (!hasChannels[p]) {
            if (p != (receiverCount - 1)) {
                // we need to output at least 1 pixel
                AddNullPixelString();
            } else {
                // in this case, we can revert and just send one less receivers of data
                m_outputChannels = mc;
                while (sz != m_virtualStrings.size()) {
                    m_virtualStrings.pop_back();
                }
            }
        }
    }

    if (!hasChannels[0]) {
        // empty, no strings
        m_outputChannels = 0;
        m_virtualStrings.clear();
        AddVirtualString(VirtualString());
    }
    if (pinConfig) {
        OutputMonitor::INSTANCE.AddPortConfiguration(m_portNumber, *pinConfig, m_outputChannels > 0);
    }

    m_outputMap.resize(m_outputChannels);

    // Initialize all maps to an unused location which should be zero.
    // We need this so that null nodes in the middle of a string are sent
    // all zeroes to keep them dark.
    for (int i = 0; i < m_outputChannels; i++) {
        m_outputMap[i] = FPPD_OFF_CHANNEL;
    }

    int offset = 0;
    int mapIndex = 0;

    m_brightnessMaps = (uint8_t**)calloc(1, sizeof(uint8_t*) * m_outputChannels);

    for (int i = 0; i < m_virtualStrings.size(); i++) {
        m_virtualStrings[i].chMap = &m_outputMap[offset];
        int start = offset;
        offset += m_virtualStrings[i].startNulls * m_virtualStrings[i].channelsPerNode();
        SetupMap(offset, m_virtualStrings[i]);
        offset += m_virtualStrings[i].pixelCount * m_virtualStrings[i].channelsPerNode();
        offset += m_virtualStrings[i].endNulls * m_virtualStrings[i].channelsPerNode();
        m_virtualStrings[i].chMapCount = offset - start;
        for (int j = 0; j < ((m_virtualStrings[i].startNulls * m_virtualStrings[i].channelsPerNode()) + (m_virtualStrings[i].pixelCount * m_virtualStrings[i].channelsPerNode()) + (m_virtualStrings[i].endNulls * m_virtualStrings[i].channelsPerNode())); j++)
            m_brightnessMaps[mapIndex++] = m_virtualStrings[i].brightnessMap;
    }
    // turn gpio off after all channels on this port are done
    if (m_outputChannels) {
        m_gpioCommands.push_back(GPIOCommand(m_portNumber, m_outputChannels));
    }

    // certain testing capabilities (like pixel counting) may require a
    // buffer larger than the number of pixels configured
    int obs = std::max(2400, m_outputChannels);
    m_outputBuffer = (uint8_t*)calloc(obs, 1);

    if (pinConfig && pinConfig->isMember("inverted") && (*pinConfig)["inverted"].asBool()) {
        invertOutput();
    }

    return 1;
}

void PixelString::AddVirtualString(const VirtualString& vs) {
    m_outputChannels += vs.startNulls * vs.channelsPerNode();
    m_outputChannels += vs.pixelCount * vs.channelsPerNode();
    m_outputChannels += vs.endNulls * vs.channelsPerNode();

    m_virtualStrings.push_back(vs);
}

int PixelString::ReadVirtualString(Json::Value& vsc, VirtualString& vs) const {
    vs.startChannel = vsc["startChannel"].asInt();
    vs.pixelCount = vsc["pixelCount"].asInt();
    vs.groupCount = vsc["groupCount"].asInt();
    vs.reverse = vsc["reverse"].asInt();
    vs.startNulls = vsc["nullNodes"].asInt();
    if (vsc.isMember("description")) {
        vs.description = vsc["description"].asString();
    }
    if (vsc.isMember("endNulls")) {
        vs.endNulls = vsc["endNulls"].asInt();
    } else {
        vs.endNulls = 0;
    }
    vs.zigZag = vsc["zigZag"].asInt();
    vs.brightness = vsc["brightness"].asInt();
    vs.gamma = atof(vsc["gamma"].asString().c_str());

    if (vs.brightness > 100 || vs.brightness < 0) {
        vs.brightness = 100;
    }
    if (vs.gamma < 0.01) {
        vs.gamma = 0.01;
    }
    if (vs.gamma > 50.0f) {
        vs.gamma = 50.0f;
    }

    CHECKPS_SETTING(vs.pixelCount < 0);
    CHECKPS_SETTING(vs.pixelCount > MAX_PIXEL_STRING_LENGTH);
    if (vs.pixelCount) {
        // only validate settings and such if there are pixels on this string
        CHECKPS_SETTING(vs.startChannel < 0);
        CHECKPS_SETTING(vs.startChannel > FPPD_MAX_CHANNELS);
        CHECKPS_SETTING(vs.startNulls < 0);
        CHECKPS_SETTING(vs.startNulls > MAX_PIXEL_STRING_LENGTH);
        CHECKPS_SETTING(vs.endNulls < 0);
        CHECKPS_SETTING(vs.endNulls > MAX_PIXEL_STRING_LENGTH);
        CHECKPS_SETTING((vs.startNulls + vs.pixelCount + vs.endNulls) > MAX_PIXEL_STRING_LENGTH);
        CHECKPS_SETTING(vs.reverse < 0);
        CHECKPS_SETTING(vs.reverse > 1);
        CHECKPS_SETTING(vs.groupCount < 0);
        CHECKPS_SETTING(vs.groupCount > vs.pixelCount);
        CHECKPS_SETTING(vs.zigZag > vs.pixelCount);
        CHECKPS_SETTING(vs.zigZag < 0);

        std::string colorOrder = vsc["colorOrder"].asString();
        if (colorOrder.length() == 4) {
            if (colorOrder[0] == 'W') {
                vs.whiteOffset = 0;
                colorOrder = colorOrder.substr(1);
            } else {
                vs.whiteOffset = 3;
                colorOrder = colorOrder.substr(0, 3);
            }
        } else if (colorOrder.length() == 1) {
            colorOrder = "W";
        }
        vs.colorOrder = ColorOrderFromString(colorOrder);

        if (vs.groupCount == 1)
            vs.groupCount = 0;

        if ((vs.zigZag == vs.pixelCount) || (vs.zigZag == 1))
            vs.zigZag = 0;

        float bf = vs.brightness;
        float maxB = bf * 2.55f;
        for (int x = 0; x < 256; x++) {
            float f = x;
            f = maxB * pow(f / 255.0f, vs.gamma);
            if (f > 255.0) {
                f = 255.0;
            }
            if (f < 0.0) {
                f = 0.0;
            }
            vs.brightnessMap[x] = round(f);
        }
    } else {
        vs.colorOrder = ColorOrderFromString("RGB");
        vs.groupCount = 1;
        vs.zigZag = 0;
        vs.reverse = 0;
        vs.startNulls = 0;
        vs.endNulls = 0;
        vs.startChannel = 0;
        for (int x = 0; x < 256; x++) {
            vs.brightnessMap[x] = x;
        }
    }

    return 1;
}
void PixelString::AddNullPixelString() {
    VirtualString vs;
    vs.pixelCount = 1;
    vs.startChannel = FPPD_OFF_CHANNEL;
    AddVirtualString(vs);
}

void PixelString::SetupMap(int vsOffset, const VirtualString& vs) {
    if (vs.receiverNum >= 0) {
        // smart receiever, add codes to outputs
        // drop low to trigger receiver to start trying to figure out which data is next
        if (vs.leadInCount > 0) {
            m_gpioCommands.push_back(GPIOCommand(m_portNumber, vsOffset));
            for (int x = 0; x < vs.channelsPerNode() * vs.leadInCount; x++) {
                m_outputMap[vsOffset++] = FPPD_OFF_CHANNEL;
            }
            // Turn back on
            m_gpioCommands.push_back(GPIOCommand(m_portNumber, vsOffset, 1));
        }
        if (vs.toggleCount) {
            // Toggle a bunch back and forth
            for (int x = 0; x < vs.channelsPerNode() * vs.toggleCount; x++) {
                // using white allows an even up/down on the GPIO
                m_outputMap[vsOffset++] = FPPD_WHITE_CHANNEL;
            }
        }
        if (vs.leadOutCount) {
            // drop low again and turn off so receiver can enable/disable whichever outputs are needed
            m_gpioCommands.push_back(GPIOCommand(m_portNumber, vsOffset));
            for (int x = 0; x < vs.channelsPerNode() * vs.leadOutCount; x++) {
                m_outputMap[vsOffset++] = FPPD_OFF_CHANNEL;
            }
            // turn gpio back on
            m_gpioCommands.push_back(GPIOCommand(m_portNumber, vsOffset, 1));
        }
        return;
    }

    int offset = vsOffset;
    int group = 0;
    int maxGroups = 0;
    int itemsInGroup = 0;
    int ch = 0;
    int pStart = 0;
    int ch1 = 0;
    int ch2 = 0;
    int ch3 = 0;

    if (vs.groupCount) {
        maxGroups = vs.pixelCount / vs.groupCount;
    }

    for (int p = pStart; p < vs.pixelCount; p++) {
        if (vs.groupCount) {
            if (itemsInGroup >= vs.groupCount) {
                group++;
                itemsInGroup = 0;
            }

            ch = vs.startChannel + (group * vs.channelsPerNode());

            itemsInGroup++;
        } else {
            ch = vs.startChannel + (p * vs.channelsPerNode());
        }

        if (vs.colorOrder == FPPColorOrder::kColorOrderONE) {
            m_outputMap[offset++] = ch;
        } else {
            if (vs.colorOrder == FPPColorOrder::kColorOrderRGB) {
                ch1 = ch;
                ch2 = ch + 1;
                ch3 = ch + 2;
            } else if (vs.colorOrder == FPPColorOrder::kColorOrderRBG) {
                ch1 = ch;
                ch2 = ch + 2;
                ch3 = ch + 1;
            } else if (vs.colorOrder == FPPColorOrder::kColorOrderGRB) {
                ch1 = ch + 1;
                ch2 = ch;
                ch3 = ch + 2;
            } else if (vs.colorOrder == FPPColorOrder::kColorOrderGBR) {
                ch1 = ch + 2;
                ch2 = ch;
                ch3 = ch + 1;
            } else if (vs.colorOrder == FPPColorOrder::kColorOrderBRG) {
                ch1 = ch + 1;
                ch2 = ch + 2;
                ch3 = ch;
            } else if (vs.colorOrder == FPPColorOrder::kColorOrderBGR) {
                ch1 = ch + 2;
                ch2 = ch + 1;
                ch3 = ch;
            }

            if (vs.whiteOffset == 0) {
                m_outputMap[offset++] = ch;
                ch1++;
                ch2++;
                ch3++;
            }
            m_outputMap[offset++] = ch1;
            m_outputMap[offset++] = ch2;
            m_outputMap[offset++] = ch3;
            if (vs.whiteOffset == 3) {
                m_outputMap[offset++] = ch + 3;
            }
        }
        ch += vs.channelsPerNode();
    }

    DumpMap("BEFORE ZIGZAG");

    if (vs.zigZag) {
        int segment = 0;
        int pixel = 0;
        int zigChannelCount = vs.zigZag * vs.channelsPerNode();

        for (int i = 0; i < m_outputChannels; i += zigChannelCount) {
            segment = i / zigChannelCount;
            if (segment % 2) {
                int offset1 = i;
                int offset2 = i + zigChannelCount - vs.channelsPerNode();

                if ((offset2 + 2) < m_outputChannels)
                    FlipPixels(offset1, offset2, vs.channelsPerNode());
            }
        }

        DumpMap("AFTER ZIGZAG");
    }

    if (vs.reverse && (vs.pixelCount > 1)) {
        FlipPixels(vsOffset, vsOffset + (vs.pixelCount * vs.channelsPerNode()) - vs.channelsPerNode(), vs.channelsPerNode());

        DumpMap("AFTER REVERSE");
    }
}

/*
 *
 */
void PixelString::DumpMap(const char* msg) {
    if (WillLog(LOG_EXCESSIVE, VB_CHANNELOUT)) {
        LogExcess(VB_CHANNELOUT, "OutputMap: %s\n", msg);
        for (int i = 0; i < m_outputChannels; i++) {
            LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
        }
    }
}

/*
 *
 */
void PixelString::FlipPixels(int offset1, int offset2, int chanCount) {
    int ch1 = 0;
    int ch2 = 0;
    int ch3 = 0;
    int ch4 = 0;
    int flipPixels = (offset2 - offset1 + chanCount) / chanCount / 2;

    for (int i = 0; i < flipPixels; i++) {
        ch1 = m_outputMap[offset1];
        ch2 = m_outputMap[offset1 + 1];
        ch3 = m_outputMap[offset1 + 2];
        if (chanCount == 4) {
            ch4 = m_outputMap[offset1 + 3];
        }

        m_outputMap[offset1] = m_outputMap[offset2];
        m_outputMap[offset1 + 1] = m_outputMap[offset2 + 1];
        m_outputMap[offset1 + 2] = m_outputMap[offset2 + 2];
        if (chanCount == 4) {
            m_outputMap[offset1 + 3] = m_outputMap[offset2 + 3];
        }

        m_outputMap[offset2] = ch1;
        m_outputMap[offset2 + 1] = ch2;
        m_outputMap[offset2 + 2] = ch3;
        if (chanCount == 4) {
            m_outputMap[offset2 + 3] = ch4;
        }

        offset1 += chanCount;
        offset2 -= chanCount;
    }
}

/*
 *
 */
void PixelString::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "        port number      : %d\n", m_portNumber);

    int count = 0;
    for (auto& vs : m_virtualStrings) {
        if (vs.receiverNum == -1) {
            LogDebug(VB_CHANNELOUT, "        --- Virtual String #%d ---\n", ++count);
            LogDebug(VB_CHANNELOUT, "        description   : %s\n", vs.description.c_str());
            LogDebug(VB_CHANNELOUT, "        start channel : %d\n", vs.startChannel + m_channelOffset);
            LogDebug(VB_CHANNELOUT, "        pixel count   : %d\n", vs.pixelCount);
            LogDebug(VB_CHANNELOUT, "        group count   : %d\n", vs.groupCount);
            LogDebug(VB_CHANNELOUT, "        reverse       : %d\n", vs.reverse);
            if (vs.whiteOffset == 0) {
                LogDebug(VB_CHANNELOUT, "        color order   : W%s\n", ColorOrderToString(vs.colorOrder).c_str());
            } else if (vs.whiteOffset == 0) {
                LogDebug(VB_CHANNELOUT, "        color order   : %sW\n", ColorOrderToString(vs.colorOrder).c_str());
            } else {
                LogDebug(VB_CHANNELOUT, "        color order   : %s\n", ColorOrderToString(vs.colorOrder).c_str());
            }
            LogDebug(VB_CHANNELOUT, "        start nulls   : %d\n", vs.startNulls);
            LogDebug(VB_CHANNELOUT, "        end nulls     : %d\n", vs.endNulls);
            LogDebug(VB_CHANNELOUT, "        zig zag       : %d\n", vs.zigZag);
            LogDebug(VB_CHANNELOUT, "        brightness    : %d\n", vs.brightness);
            LogDebug(VB_CHANNELOUT, "        gamma         : %.3f\n", vs.gamma);
        }
    }
}

void PixelString::invertOutput() {
    m_isInverted = !m_isInverted;
    for (auto& vs : m_virtualStrings) {
        for (int x = 0; x < 256; x++) {
            vs.brightnessMap[x] = ~vs.brightnessMap[x];
        }
    }
}

void PixelString::AutoCreateOverlayModels(const std::vector<PixelString*>& strings) {
    if (PixelOverlayManager::INSTANCE.isAutoCreatePixelOverlayModels()) {
        std::map<std::string, std::vector<VirtualString*>> vstrings;
        for (int s = 0; s < strings.size(); s++) {
            char vsnum = 'A';
            int vsidx = 0;
            for (int vs = 0; vs < strings[s]->m_virtualStrings.size(); vs++) {
                std::string desc = strings[s]->m_virtualStrings[vs].description;
                if (desc == "") {
                    desc = "String-" + std::to_string(s + 1);
                    if (strings[s]->m_virtualStrings.size() > 1) {
                        desc.append(1, vsnum);
                        desc += std::string("-");
                        desc += std::to_string(vsidx + 1);
                        if (strings[s]->m_virtualStrings[vs].receiverNum == -1) {
                            vsidx++;
                        } else if (vs != 0) {
                            vsidx = 0;
                            vsnum++;
                        }
                    }
                }
                // xLights will name the individual strings of a matrix/prop/model with a -str-# postfix
                // so we will try and detect this and recreate the original model
                size_t found = desc.find("-str-");
                if (found == std::string::npos) {
                    vstrings[desc].push_back(&strings[s]->m_virtualStrings[vs]);
                } else {
                    int idx = std::stoi(desc.substr(found + 5));
                    if (idx > 0)
                        --idx;
                    desc = desc.substr(0, found);
                    if (vstrings[desc].size() <= idx) {
                        vstrings[desc].resize(idx + 1);
                    }
                    vstrings[desc][idx] = &strings[s]->m_virtualStrings[vs];
                }
            }
        }
        for (auto& m : vstrings) {
            if (PixelOverlayManager::INSTANCE.getModel(m.first) != nullptr) {
                continue;
            }
            std::string name = m.first;
            auto& vs = m.second;

            uint32_t startChannel = FPPD_MAX_CHANNELS;
            uint32_t channelsPerNode = 0;
            std::string orientation = "H";
            std::string startLocation = "BL";
            uint32_t strings = vs.size();
            uint32_t strands = 1;
            uint32_t maxChan = 0;
            int8_t rn = -1;
            for (auto& a : vs) {
                if (a) {
                    startChannel = std::min(startChannel, (uint32_t)a->startChannel);
                    maxChan = a->startChannel + (a->pixelCount * a->channelsPerNode() / (a->groupCount ? a->groupCount : 1));
                    channelsPerNode = std::max(channelsPerNode, (uint32_t)a->channelsPerNode());
                    rn = std::max(rn, a->receiverNum);
                } else {
                    --strings;
                }
            }
            int32_t channelCount = maxChan - startChannel;

            if (name.find("Tree") != std::string::npos || name.find("TREE") != std::string::npos || name.find("tree") != std::string::npos || name.find("Vert") != std::string::npos || name.find("vert") != std::string::npos) {
                // some common names that are usually vertical oriented
                orientation = std::string("V");
            }

            if ((channelCount > 0) && (rn == -1)) {
                PixelOverlayManager::INSTANCE.addAutoOverlayModel(name, startChannel, channelCount, channelsPerNode, orientation,
                                                                  startLocation, strings, strands);
            }
        }
    }
}

uint8_t* PixelString::prepareOutput(uint8_t* channelData) {
    int idx = 0;
    for (auto& vs : m_virtualStrings) {
        int* map = vs.chMap;
        uint8_t* brightness = vs.brightnessMap;
        for (int ch = 0; ch < vs.chMapCount; ch++) {
            m_outputBuffer[idx++] = brightness[channelData[map[ch]]];
        }
    }
    return m_outputBuffer;
}
