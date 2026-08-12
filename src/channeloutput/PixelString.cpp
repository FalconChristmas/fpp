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

#include "fpp-json.h"

// not in fpp-pch.h; libc++ pulls them in transitively but libstdc++ does not
#include <memory>
#include <tuple>

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
        WarningHolder::AddWarning(26, "Invalid PixelString Config: " + std::string(#SETTING)); \
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
    // never leave this dangling - ReadVirtualString normally replaces it, but
    // a default constructed string is used directly for empty ports
    brightnessMap = BrightnessLUTCache::get(100, 1.0f, false);
}

VirtualString::VirtualString(int rt, int rn) :
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
    // identity: full brightness, no gamma
    brightnessMap = BrightnessLUTCache::get(100, 1.0f, false);
}
namespace {
struct LUTKey {
    int brightness;
    int gammaScaled;
    bool inverted;
    bool operator<(const LUTKey& o) const {
        return std::tie(brightness, gammaScaled, inverted) <
               std::tie(o.brightness, o.gammaScaled, o.inverted);
    }
};
std::map<LUTKey, std::unique_ptr<std::array<uint8_t, 256>>> g_luts;
std::map<LUTKey, std::unique_ptr<std::array<uint16_t, 256>>> g_wideLuts;
std::mutex g_lutLock;

// gamma is quantised so it can key the cache; build from the quantised value
// so two strings sharing a key get byte-identical tables whichever is built
// first.  1/10000 is past any configurable gamma - 2.2f round trips exactly.
int scaleGamma(float gamma) {
    return (int)std::lround(gamma * 10000.0f);
}
} // namespace

const uint8_t* BrightnessLUTCache::get(int brightness, float gamma, bool inverted) {
    // Quantise gamma so it can key the cache, then build from the quantised
    // value rather than the caller's: two strings that share a key must get
    // byte-identical tables whichever of them is built first.  1/10000 is fine
    // past any gamma anyone configures - 2.2f round trips exactly.
    int gammaScaled = scaleGamma(gamma);
    LUTKey key{ brightness, gammaScaled, inverted };

    std::lock_guard<std::mutex> lock(g_lutLock);
    auto it = g_luts.find(key);
    if (it != g_luts.end()) {
        return it->second->data();
    }

    auto table = std::make_unique<std::array<uint8_t, 256>>();
    // identical to the original per-string computation, so the tables are
    // byte for byte what they always were (at 100/1.0 that is the identity
    // map, which is what the smart receiver markers want)
    float maxB = brightness * 2.55f;
    float g = gammaScaled / 10000.0f;
    for (int x = 0; x < 256; x++) {
        float f = maxB * pow((float)x / 255.0f, g);
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        uint8_t v = (uint8_t)round(f);
        (*table)[x] = inverted ? (uint8_t)~v : v;
    }
    const uint8_t* p = table->data();
    g_luts[key] = std::move(table);
    return p;
}

const uint16_t* BrightnessLUTCache::getWide(int brightness, float gamma, bool inverted) {
    int gammaScaled = scaleGamma(gamma);
    LUTKey key{ brightness, gammaScaled, inverted };

    std::lock_guard<std::mutex> lock(g_lutLock);
    auto it = g_wideLuts.find(key);
    if (it != g_wideLuts.end()) {
        return it->second->data();
    }

    auto table = std::make_unique<std::array<uint16_t, 256>>();
    // the same curve as the 8 bit table, evaluated to full 16 bit range
    float maxB = brightness * 655.35f;
    float g = gammaScaled / 10000.0f;
    for (int x = 0; x < 256; x++) {
        float f = maxB * pow((float)x / 255.0f, g);
        if (f > 65535.0) {
            f = 65535.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        uint16_t v = (uint16_t)llround(f);
        (*table)[x] = inverted ? (uint16_t)~v : v;
    }
    const uint16_t* p = table->data();
    g_wideLuts[key] = std::move(table);
    return p;
}

size_t BrightnessLUTCache::tableCount() {
    std::lock_guard<std::mutex> lock(g_lutLock);
    return g_luts.size() + g_wideLuts.size();
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
    return colorOrder.channelCount();
}

/*
 *
 */
PixelString::PixelString(bool supportSmart) :
    m_portNumber(0),
    m_channelOffset(0),
    m_outputBytes(0),
    m_isSmartReceiver(supportSmart),
    m_brightnessMaps(nullptr),
    m_outputBuffer(nullptr) {
}

/*
 *
 */
PixelString::~PixelString() {
    if (!m_pinConfig.empty()) {
        OutputMonitor::INSTANCE.RemovePortConfiguration(m_portNumber, m_pinConfig);
    }
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
    m_outputBytes = 0;
    // needed before the virtual strings are read so their lookup tables are
    // built at the right width
    m_bytesPerChannel = protocolBytesPerChannel(config["protocol"].asString());

    int receiverCount = 1;
    if (m_isSmartReceiver && config.isMember("differentialType")) {
        smartReceiverType = ReceiverType::Standard;
        int rt = config["differentialType"].asInt();
        if (rt) {
            if (rt <= 3) {
                // v1 smart receivers
                receiverCount = rt;
                smartReceiverType = ReceiverType::v1;
            } else if (rt > 15) {
                // Falcon V4 smart receivers (send-only config packet)
                receiverCount = rt - 15;
                smartReceiverType = ReceiverType::FalconV4;
            } else if (rt > 9) {
                // Falcon V5 smart receivers (bidirectional)
                receiverCount = rt - 9;
                smartReceiverType = ReceiverType::FalconV5;
            } else {
                // v2 smart receivers
                receiverCount = rt - 3;
                smartReceiverType = ReceiverType::v2;
            }
        }
        m_isSmartReceiver = smartReceiverType != ReceiverType::Standard;
        if (m_bytesPerChannel != 1 &&
            (smartReceiverType == ReceiverType::v1 || smartReceiverType == ReceiverType::v2)) {
            // v1/v2 receivers interleave fixed width marker patterns with the
            // pixel data, so a wide part would need two widths in one stream.
            // Falcon v4/v5 markers are zero length, so those are fine.
            LogWarn(VB_CHANNELOUT, "16 bit protocols are not supported with v1/v2 smart receivers on port %d; using 8 bit\n", m_portNumber + 1);
            WarningHolder::AddWarning("PixelString: 16 bit protocol not supported with v1/v2 smart receivers on port " + std::to_string(m_portNumber + 1));
            m_bytesPerChannel = 1;
        }
    } else {
        smartReceiverType = ReceiverType::None;
        m_isSmartReceiver = false;
    }
    if (smartReceiverType == ReceiverType::v1) {
        // v1 needs a lead in
        AddVirtualString(VirtualString(0, 0));
    }
    int startMaxChan = m_outputBytes;
    for (int i = 0; i < config["virtualStrings"].size(); i++) {
        Json::Value vsc = config["virtualStrings"][i];
        VirtualString vs;
        if (!ReadVirtualString(vsc, vs)) {
            return 0;
        }
        AddVirtualString(vs);
    }
    std::array<bool, 6> hasChannels;
    hasChannels[0] = startMaxChan != m_outputBytes;
    if (!hasChannels[0] && (smartReceiverType == ReceiverType::v1 || smartReceiverType == ReceiverType::v2)) {
        // we need to output at least 1 pixel
        AddNullPixelString();
    }
    for (int p = 1; p < receiverCount; p++) {
        int sz = m_virtualStrings.size();
        int mc = m_outputBytes;

        if (smartReceiverType == ReceiverType::v1) {
            // v1 smart  receiver
            AddVirtualString(VirtualString(p, p));
        } else if (smartReceiverType == ReceiverType::v2) {
            // v2 smart  receiver, .15ms gap
            AddVirtualString(VirtualString(3, p));
        } else if (smartReceiverType == ReceiverType::FalconV5 || smartReceiverType == ReceiverType::FalconV4) {
            // Falcon v4/v5, just marker to determine config packets later
            AddVirtualString(VirtualString(4, p));
        }
        startMaxChan = m_outputBytes;
        for (int i = 0; i < config[SMART_RECEIVER_LABELS[p]].size(); i++) {
            Json::Value vsc = config[SMART_RECEIVER_LABELS[p]][i];
            VirtualString vs;
            if (!ReadVirtualString(vsc, vs)) {
                return 0;
            }
            AddVirtualString(vs);
        }
        hasChannels[p] = startMaxChan != m_outputBytes;
        hasChannels[0] |= hasChannels[p];
        if (!hasChannels[p]) {
            if (p != (receiverCount - 1)) {
                // we need to output at least 1 pixel
                AddNullPixelString();
            } else {
                // in this case, we can revert and just send one less receivers of data
                m_outputBytes = mc;
                while (sz != m_virtualStrings.size()) {
                    m_virtualStrings.pop_back();
                }
            }
        }
    }

    if (!hasChannels[0]) {
        // empty, no strings
        m_outputBytes = 0;
        m_virtualStrings.clear();
        AddVirtualString(VirtualString());
    }
    if (pinConfig) {
        m_pinConfig = *pinConfig;
        OutputMonitor::INSTANCE.AddPortConfiguration(m_portNumber, m_pinConfig, m_outputBytes > 0);
    }

    m_outputMap.resize(m_outputBytes);

    // Initialize all maps to an unused location which should be zero.
    // We need this so that null nodes in the middle of a string are sent
    // all zeroes to keep them dark.
    for (int i = 0; i < m_outputBytes; i++) {
        m_outputMap[i] = FPPD_OFF_CHANNEL;
    }

    int offset = 0;
    int mapIndex = 0;

    m_brightnessMaps = (const uint8_t**)calloc(1, sizeof(const uint8_t*) * m_outputBytes);

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
    // A 16 bit part clocks two bytes per colour channel.  The channel maps
    // above are per channel and stay as they are - only the wire length grows.
    // Marker strings from a smart receiver are raw wire bytes and do not
    // widen, but the only receivers allowed here (Falcon v4/v5) have zero
    // length markers, so this is a straight doubling.
    if (m_bytesPerChannel != 1) {
        m_outputBytes *= m_bytesPerChannel;
    }

    // Protocol preamble, if any.  It has to land after the channel maps are
    // built (those are sized for the pixel bytes only) and before the gpio off
    // command below, which needs the full wire length.
    if (m_outputBytes && protocolNeedsTM1814Preamble(config["protocol"].asString())) {
        // per channel drive current; the part powers up at its 6.5mA floor, so
        // without this a TM1814 string just looks dim
        float mA = config.isMember("tm1814MilliAmps") ? config["tm1814MilliAmps"].asFloat() : 38.0f;
        m_preamble = buildTM1814Preamble(mA);
        m_outputBytes += m_preamble.size();
    }

    // turn gpio off after all channels on this port are done
    if (m_outputBytes) {
        m_gpioCommands.push_back(GPIOCommand(m_portNumber, m_outputBytes));
    }

    // certain testing capabilities (like pixel counting) may require a
    // buffer larger than the number of pixels configured
    int obs = std::max(2400, m_outputBytes);
    m_outputBuffer = (uint8_t*)calloc(obs, 1);

    // Cape declared inversion corrects a wiring defect; protocol inversion is
    // the pixel type asking for it.  Both flip the same line, so they compose
    // by xor and cancel when a TM18xx string lands on a backwards-wired port.
    m_protocolInverted = isInvertedProtocol(config["protocol"].asString());
    // isMember first: pinConfig points into the cape config and a bare
    // operator[] would insert a null member into it
    bool wireInverted = pinConfig && pinConfig->isMember("inverted") && (*pinConfig)["inverted"].asBool();
    if (m_protocolInverted != wireInverted) {
        invertOutput();
    }

    // after any inversion, so the bytes copied in are the wire form
    if (!m_preamble.empty()) {
        memcpy(m_outputBuffer, m_preamble.data(), m_preamble.size());
    }

    return 1;
}

// TM1814 frames are "C1 C2 D1..Dn Reset".  C1 carries four 6 bit constant
// current levels in the chip's physical W,R,G,B pin order with the top two
// bits of each byte fixed at 0; C2 is the bitwise complement of C1 and the
// chip will not decode the frame without it.  Every chip in the chain reads
// and forwards both, so this is once per frame, not once per pixel.
std::vector<uint8_t> PixelString::buildTM1814Preamble(float milliAmps) {
    // 64 levels spanning 6.5mA (all zero) to 38mA (all ones)
    constexpr float MIN_MA = 6.5f;
    constexpr float MAX_MA = 38.0f;
    milliAmps = std::clamp(milliAmps, MIN_MA, MAX_MA);
    uint8_t level = (uint8_t)std::lround((milliAmps - MIN_MA) * 63.0f / (MAX_MA - MIN_MA)) & 0x3F;

    std::vector<uint8_t> p(8);
    // C1: W, R, G, B - all four channels get the same level.  This is the
    // chip's pin order and is independent of the string's color order, which
    // only decides how channel data lands in the W/R/G/B slots of Dn.
    for (int i = 0; i < 4; i++) {
        p[i] = level;
        p[i + 4] = ~level;
    }
    return p;
}

int PixelString::protocolBytesPerChannel(const std::string& protocol) {
    // single wire 16 bit parts; bit depths follow the pixel table in xLights
    // (src-core/models/Pixels.cpp).  The bare name is the 16 bit part itself,
    // and the explicit "16 bit"/"(16)" spellings some controllers use mean the
    // same thing here.
    if (protocol == "ucs8903" || protocol == "ucs8904" || protocol == "ucs7604" ||
        protocol == "ucs8903 16 bit" || protocol == "ucs8904 16 bit" ||
        protocol == "ucs7604 16 bit") {
        return 2;
    }
    return 1;
}

bool PixelString::protocolNeedsTM1814Preamble(const std::string& protocol) {
    return protocol == "tm1814" || protocol == "tm1814a";
}

void PixelString::setPixelDataChannels(int channels) {
    // a port clamped to nothing sends nothing, preamble included - the same
    // rule Init uses when deciding to build one at all
    m_outputBytes = channels ? channels * m_bytesPerChannel + (int)m_preamble.size() : 0;
}

void PixelString::dropPreamble() {
    if (m_preamble.empty()) {
        return;
    }
    int n = (int)m_preamble.size();
    m_outputBytes -= n;
    // the preamble shifted the whole stream, so every command past it moves back
    for (auto& c : m_gpioCommands) {
        if (c.channelOffset >= n) {
            c.channelOffset -= n;
        }
    }
    m_preamble.clear();
    memset(m_outputBuffer, 0, n);
}

bool PixelString::isInvertedProtocol(const std::string& protocol) {
    // Only the protocols FPP can actually drive today.  The TM18xx bit cell
    // is close enough to ws2811 that the shared timing covers it; what makes
    // them different is the idle level.
    return protocol == "tm1814" || protocol == "tm1814a";
}

void PixelString::AddVirtualString(const VirtualString& vs) {
    m_outputBytes += vs.startNulls * vs.channelsPerNode();
    m_outputBytes += vs.pixelCount * vs.channelsPerNode();
    m_outputBytes += vs.endNulls * vs.channelsPerNode();

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
        vs.colorOrder = ColorOrderFromString(colorOrder);

        if (vs.groupCount == 1)
            vs.groupCount = 0;

        if ((vs.zigZag == vs.pixelCount) || (vs.zigZag == 1))
            vs.zigZag = 0;

        vs.brightnessMap = BrightnessLUTCache::get(vs.brightness, vs.gamma, false);
        if (m_bytesPerChannel == 2) {
            vs.brightnessMap16 = BrightnessLUTCache::getWide(vs.brightness, vs.gamma, false);
        }
    } else {
        vs.colorOrder = ColorOrderFromString("RGB");
        vs.groupCount = 1;
        vs.zigZag = 0;
        vs.reverse = 0;
        vs.startNulls = 0;
        vs.endNulls = 0;
        vs.startChannel = 0;
        vs.brightnessMap = BrightnessLUTCache::get(100, 1.0f, false);
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

    int redOffset = vs.colorOrder.redOffset();
    int greenOffset = vs.colorOrder.greenOffset();
    int blueOffset = vs.colorOrder.blueOffset();
    int whiteOffset = vs.colorOrder.whiteOffset();

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
            // incoming data is always RGB or RGBW, map to whatever order is needed
            if (whiteOffset == 0) {
                // white is first, then color order on the way out, but W is always data[3]
                m_outputMap[offset++] = ch + 3;
            }
            m_outputMap[offset + redOffset] = ch;       // red is always data[0]
            m_outputMap[offset + greenOffset] = ch + 1; // green is always data[1]
            m_outputMap[offset + blueOffset] = ch + 2;  // blue is always data[2]
            offset += 3;
            if (whiteOffset == 3) {
                m_outputMap[offset++] = ch + 3;
            }
        }
    }

    DumpMap("BEFORE ZIGZAG");

    if (vs.zigZag) {
        int segment = 0;
        int pixel = 0;
        int zigChannelCount = vs.zigZag * vs.channelsPerNode();

        for (int i = 0; i < m_outputBytes; i += zigChannelCount) {
            segment = i / zigChannelCount;
            if (segment % 2) {
                int offset1 = i;
                int offset2 = i + zigChannelCount - vs.channelsPerNode();

                if ((offset2 + 2) < m_outputBytes)
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
        for (int i = 0; i < m_outputBytes; i++) {
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
            LogDebug(VB_CHANNELOUT, "        color order   : %s\n", ColorOrderToString(vs.colorOrder).c_str());
            LogDebug(VB_CHANNELOUT, "        start nulls   : %d\n", vs.startNulls);
            LogDebug(VB_CHANNELOUT, "        end nulls     : %d\n", vs.endNulls);
            LogDebug(VB_CHANNELOUT, "        zig zag       : %d\n", vs.zigZag);
            LogDebug(VB_CHANNELOUT, "        brightness    : %d\n", vs.brightness);
            LogDebug(VB_CHANNELOUT, "        gamma         : %.3f\n", vs.gamma);
        }
    }
}

// Everything downstream of here holds wire form, not logical form: on an
// inverted port the line idles high and the data phase drives high for a
// logical zero, so every byte that goes out has to be complemented.  The
// brightness maps carry that for pixel data; the preamble needs it too.
void PixelString::invertOutput() {
    m_isInverted = !m_isInverted;
    for (auto& vs : m_virtualStrings) {
        // the tables are shared, so swap to the complemented one rather than
        // flipping this string's in place and corrupting every other user
        int b = vs.isSmartReceiver() ? 100 : vs.brightness;
        float g = vs.isSmartReceiver() ? 1.0f : vs.gamma;
        vs.brightnessMap = BrightnessLUTCache::get(b, g, m_isInverted);
        if (vs.brightnessMap16) {
            vs.brightnessMap16 = BrightnessLUTCache::getWide(b, g, m_isInverted);
        }
    }
    for (auto& b : m_preamble) {
        b = ~b;
    }
}

void PixelString::AutoCreateOverlayModels(const std::vector<PixelString*>& strings, std::list<std::string>& autoModelNames) {
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
            // Channel data is always in R,G,B[,W] order; the output driver remaps to the
            // hardware wire order.  The model's ColorOrder describes the channel data.
            std::string colorOrder = (channelsPerNode >= 4) ? "RGBW" : "RGB";
            int32_t channelCount = maxChan - startChannel;

            if (name.find("Tree") != std::string::npos || name.find("TREE") != std::string::npos || name.find("tree") != std::string::npos || name.find("Vert") != std::string::npos || name.find("vert") != std::string::npos) {
                // some common names that are usually vertical oriented
                orientation = std::string("V");
            }

            if ((channelCount > 0) && (rn == -1)) {
                autoModelNames.push_back(name);
                PixelOverlayManager::INSTANCE.addAutoOverlayModel(name, startChannel, channelCount, channelsPerNode, orientation,
                                                                  startLocation, strings, strands, colorOrder);
            }
        }
    }
}

uint8_t* PixelString::prepareOutput(uint8_t* channelData) {
    // any protocol preamble is already sitting at the head of the buffer
    int idx = m_preamble.size();
    for (auto& vs : m_virtualStrings) {
        int* map = vs.chMap;
        if (vs.brightnessMap16) {
            // 16 bit parts take each channel most significant byte first
            const uint16_t* brightness = vs.brightnessMap16;
            for (int ch = 0; ch < vs.chMapCount; ch++) {
                uint16_t v = brightness[channelData[map[ch]]];
                m_outputBuffer[idx++] = (uint8_t)(v >> 8);
                m_outputBuffer[idx++] = (uint8_t)v;
            }
        } else {
            const uint8_t* brightness = vs.brightnessMap;
            for (int ch = 0; ch < vs.chMapCount; ch++) {
                m_outputBuffer[idx++] = brightness[channelData[map[ch]]];
            }
        }
    }
    return m_outputBuffer;
}
