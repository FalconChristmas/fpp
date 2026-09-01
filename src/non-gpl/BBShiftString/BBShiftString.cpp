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

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <arm_neon.h>
#include <cstring>
#include <tuple>

#include <chrono>

// FPP includes
#include "../../Sequence.h"
#include "../../channeloutput/channeloutputthread.h"
#include "../../Warnings.h"
#include "../../common.h"
#include "../../log.h"

#include "BBShiftString.h"
#include "../CapeUtils/CapeUtils.h"
#include "../../pru/SMEMRing.hp"

#include "../../overlays/PixelOverlay.h"

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

// r30 bit numbers of the clock/latch pins, matching the compile-time
// defaults in BBShiftString.asm (CONTROL_BIT_BASE + CLOCK_PIN/LATCH_PIN);
// a cape's pruPinConfig clockPin/latchPin names override them and the
// resolved values are published to the firmware at PINCFG_OFFSET
#ifdef PLATFORM_BBB
static const int DEFAULT_CLOCK_BIT[2] = { 15, 10 };
static const int DEFAULT_LATCH_BIT[2] = { 14, 11 };
#else
static const int DEFAULT_CLOCK_BIT[2] = { 19, 19 };
static const int DEFAULT_LATCH_BIT[2] = { 16, 16 };
#endif
// must match PINCFG_OFFSET in BBShiftString.asm
#define PINCFG_OFFSET 0x1FE8

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
    // idempotent; Close() normally does this, but an output torn down without
    // one must not leave its frame rate warning stranded in the UI
    setFrameRateWarning(false);
    clearBudgetWarning();
    BBBPru::ddrRelease("BBShiftString");
    m_pumpRunning = false;
    if (m_pumpThread.joinable()) {
        m_pumpThread.join();
    }
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
    if (m_fv5PacketMem) {
        free(m_fv5PacketMem);
    }
}

// Inverted lines and Falcon smart receivers do not mix, but only for one of
// the two reasons a line can be inverted.
//
// A chain's first port carries the config packet and the receiver's processor
// watches that line, so an inverted head is refused whatever set it.  Protocol
// inversion (TM18xx) is meant to reach the pixels, so it passes through the
// receiver and is refused for every port of the chain.
//
// Cape declared inversion on a non-head port is left alone: it exists to undo
// a backwards wired differential pair, so the receiver already sees a correct
// signal.  That is the one shipped use - a batch of K16A-B boards with the
// second line of the first differential group reversed - and it has to keep
// working.
// T0, T1 and the end of the bit cell are single latch instants shared by every
// port on the PRU, so the controller can only run one bit cell at a time.  Take
// it from the ports that actually have pixels, and if they disagree, say so
// loudly and name the odd ones out rather than driving them at a timing their
// part cannot decode.
//
// Mixing cannot be papered over by picking the slower cell: a ws281x part fed a
// 2000ns one-pulse reads it as garbage, so there is no safe common value.
void BBShiftStringOutput::resolveTiming(const Json::Value& config) {
    std::map<PixelString::Timing, std::vector<int>, TimingLess> used;
    for (int i = 0; i < (int)m_strings.size(); i++) {
        if (m_strings[i]->m_outputBytes > 0) {
            used[PixelString::protocolTiming(config["outputs"][i]["protocol"].asString())]
                .push_back(m_strings[i]->m_portNumber + 1);
        }
    }
    if (used.empty()) {
        return;
    }
    // the cell the most ports want wins
    auto best = used.begin();
    for (auto it = used.begin(); it != used.end(); ++it) {
        if (it->second.size() > best->second.size()) {
            best = it;
        }
    }
    m_t0Ns = best->first.t0Ns;
    m_t1Ns = best->first.t1Ns;
    m_lowNs = best->first.periodNs;

    if (used.size() > 1) {
        std::string odd;
        for (auto& [t, ports] : used) {
            if (t == best->first) {
                continue;
            }
            for (int p : ports) {
                odd += (odd.empty() ? "" : ", ") + std::to_string(p);
            }
        }
        LogErr(VB_CHANNELOUT, "Ports on one controller must share a bit timing; using %d/%d/%dns. Port(s) %s want a different one and will not work\n",
               m_t0Ns, m_t1Ns, m_lowNs, odd.c_str());
        WarningHolder::AddWarning("BBShiftString: port(s) " + odd +
                                  " use a pixel protocol whose timing differs from the rest of the controller");
    } else if (!(best->first == PixelString::Timing{ 320, 750, 1120 })) {
        LogInfo(VB_CHANNELOUT, "BBShiftString: bit timing %d/%d/%dns\n", m_t0Ns, m_t1Ns, m_lowNs);
    }
}

void BBShiftStringOutput::demoteInvertedReceiverChains() {
    int x = 0;
    while (x < m_strings.size()) {
        PixelString* head = m_strings[x++];
        if (!head->m_isSmartReceiver ||
            (head->smartReceiverType != PixelString::ReceiverType::FalconV5 &&
             head->smartReceiverType != PixelString::ReceiverType::FalconV4)) {
            continue;
        }
        std::vector<PixelString*> chain{ head };
        for (int i = 0; i < 3 && x < m_strings.size(); ++i) {
            chain.push_back(m_strings[x++]);
        }

        std::string why;
        if (head->m_isInverted) {
            why = "its first port is inverted";
        } else {
            for (auto* p : chain) {
                if (p->m_protocolInverted) {
                    why = "port " + std::to_string(p->m_portNumber + 1) + " uses an inverted protocol";
                    break;
                }
            }
        }
        if (why.empty()) {
            continue;
        }

        LogWarn(VB_CHANNELOUT, "Falcon smart receivers on port %d disabled: %s\n",
                head->m_portNumber + 1, why.c_str());
        WarningHolder::AddWarning("BBShiftString: smart receivers on port " +
                                  std::to_string(head->m_portNumber + 1) +
                                  " disabled, " + why);
        head->smartReceiverType = PixelString::ReceiverType::None;
        head->m_isSmartReceiver = false;
    }
}

void BBShiftStringOutput::createOutputLengths(FrameData& d, const std::string& pfx) {
    // One command table record is the high mask for every string slot, then
    // the low mask for every slot, then the two byte channel offset at which
    // the next record takes over.  Each slot contributes one byte (a bitmask
    // of the eight data pins), so a record carries 2 * stringsPerPin mask
    // bytes - 16 at 8 deep, 32 at 16 deep - and BYTES_FOR_MASKS in the
    // firmware has to agree.  The 16 deep firmware loads the high half into
    // scratchpad bank 11 and the low half into bank 12; the 8 deep one loads
    // both straight into r21-r24.
    union {
        uint16_t r[2];
        uint8_t b[4];
    } r45[2 * MAX_STRINGS_PER_PIN / 4];

    if (!d.pruData) {
        return;
    }
    // registers spanned by one mask half; the low half starts at r45[nRegs]
    const int nRegs = m_stringsPerPin / 4;
    for (int x = 0; x < 2 * nRegs; x++) {
        r45[x].r[0] = 0;
        r45[x].r[1] = 0;
    }
    std::map<int, std::vector<std::tuple<int, int, GPIOCommand, bool>>> sizes;
    int bmask = 0x1;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        for (int x = 0; x < m_stringsPerPin; ++x) {
            int pc = d.stringMap[y][x];
            if (pc >= 0) {
                for (auto& a : m_strings[pc]->m_gpioCommands) {
                    sizes[a.channelOffset].push_back(std::make_tuple(y, x, a, m_strings[pc]->m_isInverted));
                }
                int breg = x % 4;
                int idx = x / 4;
                if (m_strings[pc]->m_isInverted) {
                    r45[idx + nRegs].b[breg] |= bmask;
                } else {
                    r45[idx].b[breg] |= bmask;
                }
            }
        }
        bmask <<= 1;
    }

    // need to use pru->memcpyToPRU so we'll use a temporary here
    // and it also needs to be 64 byte aligned
    const int maxEntries = (int)(sizeof(d.pruData->commandTable) / sizeof(uint16_t));
    // one record is the mask halves plus the two byte offset that introduces
    // the next one; 9 entries at 8 deep, 17 at 16 deep
    const int entriesPerRecord = 1 + 4 * nRegs;
    uint8_t* buffer = (uint8_t*)malloc(maxEntries * sizeof(uint16_t) + 256);
    uintptr_t ptr = (uintptr_t)buffer;
    ptr += 64 - (ptr % 64);
    uint16_t* commandTable = (uint16_t*)ptr;

    int curCommandTable = 0;
    auto emitMasks = [&]() {
        for (int r = 0; r < 2 * nRegs; ++r) {
            commandTable[curCommandTable++] = r45[r].r[0];
            commandTable[curCommandTable++] = r45[r].r[1];
        }
    };
    emitMasks();

    auto i = sizes.begin();
    while (i != sizes.end()) {
        uint16_t min = i->first & 0xFFFF;
        if (min <= d.maxStringLen) {
            // the table lives in the PRU's data RAM, just below the ring
            // config words - always leave room for this record and the
            // terminator.  A 16 deep cape can reach this: twice the strings
            // and near twice the record size.
            if (curCommandTable + entriesPerRecord + 1 > maxEntries) {
                LogErr(VB_CHANNELOUT, "%s: GPIO command table full at channel offset %d; later commands dropped\n",
                       pfx.c_str(), i->first);
                WarningHolder::AddWarning("BBShiftString: too many smart receiver GPIO commands to fit the PRU command table");
                break;
            }
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
            emitMasks();
        }
        i++;
    }
    commandTable[curCommandTable++] = 0xFFFF;
    // round up to nearest 64 byte boundary
    int len = (curCommandTable) * 2;
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

    // The cape's string config has to be one written for this driver.  Here an
    // output is a place in the cape's shift register chains (numeric "pru",
    // "pin" and "index"); on a BBB48String cape it is a header pin name
    // ("pin": "P8-08").  Reading a pin name as an int throws out of jsoncpp and
    // reaches the user as a bare "Value is not convertible to Int.", so name the
    // real problem instead: the config asked for the wrong driver for this cape.
    // Configs like that do arrive - xLights 2026.16 pointed an early K16A-B at
    // this driver, a revision whose eeprom predates it and so is still a
    // BBB48String pinout (and names no "driver" at all, hence the default
    // below; later revisions of the same cape do belong here).
    const int capeOutputCount = root["outputs"].size();
    for (int i = 0; i < capeOutputCount; i++) {
        // a string "pin" is the unambiguous BBB48String signature; a missing
        // one is left to the null-reads-as-0 behaviour the shift capes have
        // always relied on for their unused fields
        if (!root["outputs"][i]["pin"].isString()) {
            continue;
        }
        std::string capeDriver = root.get("driver", "BBB48String").asString();
        LogErr(VB_CHANNELOUT, "Cape %s is not a BBShiftString cape - its string configuration names header pins, so it needs the %s output type\n",
               m_subType.c_str(), capeDriver.c_str());
        WarningHolder::AddWarning("BBShiftString: " + m_subType + " needs the " + capeDriver +
                                  " output type - open the Pixel Strings page and save to correct it");
        return 0;
    }

    // A combo cape shares the PRUSS with a panel driver: this side must not
    // clear the shared RAM or the other PRU's memory on a restart, and the
    // FalconV5 listener (a PRU0 program whose capture area overlaps the
    // panel ring) is unavailable.  (Also honored from the output config for
    // bench testing.)
    m_sharedPRUSS = root["sharedPRUSS"].asBool() || config["sharedPRUSS"].asBool();

    // How deep the cape's shift register chains are.  Absent means 8, the
    // layout every cape has shipped with so far; 16 doubles the strings one
    // PRU can drive and selects the SHIFT16 firmware.
    // (Also honored from the output config, so a new cape can be brought up
    // before its eeprom is finalized.)
    m_stringsPerPin = 8;
    if (root.isMember("stringsPerPin") || config.isMember("stringsPerPin")) {
        int spp = config.isMember("stringsPerPin") ? config["stringsPerPin"].asInt() : root["stringsPerPin"].asInt();
        if (spp != 8 && spp != 16) {
            LogErr(VB_CHANNELOUT, "Cape declares stringsPerPin %d; only 8 and 16 are supported, using 8\n", spp);
            WarningHolder::AddWarning("BBShiftString: unsupported stringsPerPin, falling back to 8");
        } else {
            m_stringsPerPin = spp;
        }
    }
#ifdef PLATFORM_BBB
    if (m_stringsPerPin != 8) {
        // At 200MHz the three shift phases of a 16 deep chain come to 1045ns
        // against a 1120ns bit budget, leaving no room for the data block
        // read - and the AM335x already reaches 128 strings using both PRUs.
        LogErr(VB_CHANNELOUT, "16 strings per pin requires the AM62x; using 8\n");
        WarningHolder::AddWarning("BBShiftString: 16 strings per pin is not supported on this SBC");
        m_stringsPerPin = 8;
    }
#endif

    // Default pin sets, overridable per PRU by the cape for combo pinouts.
    // The cape only ever names header pins (P2-02 style); the mapping to
    // r30 bit numbers happens here via the per-platform pin table, so a new
    // SBC in the same form factor can reuse the same capes and eeproms
    // (exactly how the PocketBeagle2 picked up PocketBeagle1 capes).  The
    // resolved clock/latch bits are published to the firmware at runtime.
    for (int p = 0; p < 2; p++) {
        m_dataPins[p] = PRU_DATA_PINS[p];
        m_ctrlPins[p] = PRU_CTRL_PINS[p];
        m_clockBit[p] = DEFAULT_CLOCK_BIT[p];
        m_latchBit[p] = DEFAULT_LATCH_BIT[p];
        m_pinNamesOverridden[p] = false;
    }
    for (auto const& pc : root["pruPinConfig"]) {
        int p = pc["pru"].asInt();
        if (p < 0 || p > 1) {
            continue;
        }
        if (pc.isMember("dataPins")) {
            m_dataPins[p].clear();
            for (auto const& pin : pc["dataPins"]) {
                m_dataPins[p].push_back(pin.asString());
            }
            m_pinNamesOverridden[p] = true;
        }
        m_ctrlPins[p].clear();
        for (auto const& nm : { "clockPin", "latchPin" }) {
            if (pc.isMember(nm)) {
                std::string pinName = pc[nm].asString();
                m_ctrlPins[p].push_back(pinName);
                const BBBPinCapabilities* bp = (const BBBPinCapabilities*)(PinCapabilities::getPinByName(pinName).ptr());
                // pruPin() returns uint8_t; the unmapped value (-1) needs
                // the sign restored
                int bit = bp ? (int8_t)bp->pruPin(p) : -1;
                if (bit < 0) {
                    LogErr(VB_CHANNELOUT, "%s %s has no known PRU%d r30 bit\n", nm, pinName.c_str(), p);
                    WarningHolder::AddWarning("BBShiftString: unusable control pin " + pinName);
                } else if (strcmp(nm, "clockPin") == 0) {
                    m_clockBit[p] = bit;
                } else {
                    m_latchBit[p] = bit;
                }
            }
        }
    }

    int maxStringLen = 0;
    bool hasV5SR = false;
    bool hasFalconSR = false;
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s, &root["outputs"][i])) {
            return 0;
        }

        if (newString->m_outputBytes > maxStringLen) {
            maxStringLen = newString->m_outputBytes;
        }

        m_strings.push_back(newString);
    }

    resolveTiming(config);
    demoteInvertedReceiverChains();

    for (auto& a : m_strings) {
        hasV5SR |= a->smartReceiverType == PixelString::ReceiverType::FalconV5;
        hasFalconSR |= a->smartReceiverType == PixelString::ReceiverType::FalconV4 || a->smartReceiverType == PixelString::ReceiverType::FalconV5;
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

    // The full bidirectional Falcon V5 protocol needs the cape's PRU0
    // listener support (and sole ownership of the PRUSS - the listener is a
    // PRU0 program whose capture area overlaps the panel ring half of the
    // shared RAM).  Without that, V5 receivers still work as send-only V4:
    // the config packet goes out, queries and responses do not.
    supportsV5Listeners = root.isMember("falconV5ListenerConfig") && !m_sharedPRUSS;
    if (hasV5SR && !supportsV5Listeners) {
        LogWarn(VB_CHANNELOUT, "Falcon V5 (bidirectional) smart receivers are not supported on this cape%s; falling back to send-only V4 handling\n",
                m_sharedPRUSS ? " (shared PRUSS)" : "");
        for (auto& a : m_strings) {
            if (a->smartReceiverType == PixelString::ReceiverType::FalconV5) {
                a->smartReceiverType = PixelString::ReceiverType::FalconV4;
            }
        }
        hasV5SR = false;
    }
    m_hasBidirSR = hasV5SR;

    int curRecPort = -1;
    for (int x = 0; x < m_strings.size(); x++) {
        if (curRecPort == -1 && (m_strings[x]->smartReceiverType == PixelString::ReceiverType::FalconV5 ||
                                 m_strings[x]->smartReceiverType == PixelString::ReceiverType::FalconV4)) {
            curRecPort = 0;
        }
        if (m_strings[x]->m_outputBytes > 0 || curRecPort >= 0) {
            if (curRecPort == -1 && m_strings[x]->smartReceiverType != PixelString::ReceiverType::None) {
                curRecPort = m_strings[x]->m_portNumber % 4;
            }
            // need to output this pin, configure it
            if (x >= capeOutputCount) {
                // the loop above only vetted the outputs the cape declares;
                // a config with more ports than that has nowhere to send them
                // (and a bare operator[] here would read a null entry as
                // pru 0 / pin 0 / stage 0, colliding with a real port)
                LogErr(VB_CHANNELOUT, "Output %d is past the %d the %s cape declares\n",
                       x + 1, capeOutputCount, m_subType.c_str());
                WarningHolder::AddWarning("BBShiftString: output " + std::to_string(x + 1) +
                                          " is past the end of the cape's pinout");
                continue;
            }
            int pru = root["outputs"][x]["pru"].asInt();
            int pin = root["outputs"][x]["pin"].asInt();
            int pinIdx = root["outputs"][x]["index"].asInt();
            if (pinIdx < 0 || pinIdx >= m_stringsPerPin) {
                LogErr(VB_CHANNELOUT, "Output %d has shift stage index %d but the cape declares %d strings per pin\n",
                       x, pinIdx, m_stringsPerPin);
                WarningHolder::AddWarning("BBShiftString: output " + std::to_string(x) + " shift stage index out of range");
                continue;
            }
            for (auto& a : m_ctrlPins[pru]) {
                if (m_usedPins.find(a) == m_usedPins.end()) {
                    m_usedPins[a] = "pru" + std::to_string(pru) + "out";
                }
            }
            if (pin >= (int)m_dataPins[pru].size()) {
                LogErr(VB_CHANNELOUT, "Output %d references pru %d data pin %d but only %d exist\n",
                       x, pru, pin, (int)m_dataPins[pru].size());
                continue;
            }
            const std::string& pinName = m_dataPins[pru][pin];
            if (m_usedPins.find(pinName) == m_usedPins.end()) {
                m_usedPins[pinName] = "pru" + std::to_string(pru) + "out";
            }
            // with the default pin sets the cape's pin index is the r30 bit;
            // cape-named pins resolve through the platform pin table
            int bit = pin;
            if (m_pinNamesOverridden[pru]) {
                const BBBPinCapabilities* bp = (const BBBPinCapabilities*)(PinCapabilities::getPinByName(pinName).ptr());
                bit = bp ? (int8_t)bp->pruPin(pru) : -1;
                if (bit < 0 || bit >= MAX_PINS_PER_PRU) {
                    LogErr(VB_CHANNELOUT, "Data pin %s maps to PRU%d r30 bit %d; the firmware supports bits 0-%d\n",
                           pinName.c_str(), pru, bit, MAX_PINS_PER_PRU - 1);
                    WarningHolder::AddWarning("BBShiftString: unusable data pin " + pinName);
                    continue;
                }
            }

            // printf("pru: %d  pin: %d  idx: %d\n", pru, pin, pinIdx);
            if (x >= m_licensedOutputs && m_strings[x]->m_outputBytes > 0) {
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
                m_strings[x]->setPixelDataChannels(chanCount);
            }

            if (pru == 0) {
                m_pru0.stringMap[bit][pinIdx] = x;
                m_pru0.maxStringLen = std::max(m_pru0.maxStringLen, m_strings[x]->m_outputBytes);
            } else {
                m_pru1.stringMap[bit][pinIdx] = x;
                m_pru1.maxStringLen = std::max(m_pru1.maxStringLen, m_strings[x]->m_outputBytes);
            }
            if (curRecPort >= 0) {
                if (++curRecPort == 4) {
                    curRecPort = -1;
                }
            }
        }
    }
    if (hasV5SR && m_pru1.maxStringLen <= m_pru0.maxStringLen) {
        // pru1 controls the reading mux pins so it has to output more pixels than pru0 so it knows pru0 is done
        m_pru1.maxStringLen = m_pru0.maxStringLen + 1;
    }

    int maxLen = std::max(m_pru0.maxStringLen, m_pru1.maxStringLen);
    if (maxLen > 0) {
        // 10us/byte at 800KHz plus ~1700us of measured per-frame overhead
        // (reset latch + packet staging; measured on a K32-Max, see #2855)
        m_frameTimeUs = maxLen * 10 + 1700;
        LogInfo(VB_CHANNELOUT, "BBShiftString: longest string %d bytes -> %.1fms/frame, sustainable ceiling ~%.4g fps\n",
                maxLen, m_frameTimeUs / 1000.0, 1000000.0 / m_frameTimeUs);
    }

    if (!StartPRU()) {
        return 0;
    }

#ifdef PLATFORM_BBB
    // give each area two chunks (frame flipping) of DDR memory, from the
    // shared region allocator so other outputs on the region cannot
    // overlap us; the FalconV5 packet area rides at the end
    m_pru0.frameSize = stringsPerPru() * std::max(2400, m_pru0.maxStringLen);
    // leave a full memory page between to avoid conflicts
    int offset0 = ((m_pru0.frameSize / 4096) + 2) * 4096;
    m_pru1.frameSize = stringsPerPru() * std::max(2400, m_pru1.maxStringLen);
    int offset1 = ((m_pru1.frameSize / 4096) + 2) * 4096;
    size_t v5Size = hasFalconSR ? 128 * 1024 : 0;
    uint32_t ddrPhys = 0;
    uint8_t* start = BBBPru::ddrAlloc("BBShiftString", 2 * offset0 + 2 * offset1 + v5Size, ddrPhys);
    if (!start) {
        LogErr(VB_CHANNELOUT, "BBShiftString: no room in the PRU DDR region\n");
        WarningHolder::AddWarning(20, "BBShiftString: no room in the PRU DDR region - reduce string lengths or other outputs' buffers");
        return 0;
    }
    m_pru0.curData = start;
    m_pru0.lastData = m_pru0.curData + offset0;

    m_pru0.formattedData = (uint8_t*)calloc(1, m_pru0.frameSize);

    m_pru1.curData = m_pru0.lastData + offset0;
    m_pru1.lastData = m_pru1.curData + offset1;

    m_pru1.formattedData = (uint8_t*)calloc(1, m_pru1.frameSize);
#else
    // AM62x: the frame flipping buffers live in normal cached memory; the
    // pump thread streams them into the PRU shared memory ring
    m_pru0.frameSize = stringsPerPru() * std::max(2400, m_pru0.maxStringLen);
    m_pru0.curData = (uint8_t*)calloc(1, m_pru0.frameSize);
    m_pru0.lastData = (uint8_t*)calloc(1, m_pru0.frameSize);
    m_pru0.heapData = true;
    m_pru0.formattedData = (uint8_t*)calloc(1, m_pru0.frameSize);

    m_pru1.frameSize = stringsPerPru() * std::max(2400, m_pru1.maxStringLen);
    m_pru1.curData = (uint8_t*)calloc(1, m_pru1.frameSize);
    m_pru1.lastData = (uint8_t*)calloc(1, m_pru1.frameSize);
    m_pru1.heapData = true;
    m_pru1.formattedData = (uint8_t*)calloc(1, m_pru1.frameSize);
#endif

    if (supportsV5Listeners && hasV5SR) {
        // if the cape supports v5 listeners, the enable pin needs to be
        // configured or data won't be sent on port1 of each receiver
        PinCapabilities::getPinByName(PRU1_ENABLE_PIN).configPin("pru1out", true, "BBShiftString-Enable");
    }
    if (hasFalconSR) {
#ifdef PLATFORM_BBB
        setupFalconV5Support(root, m_pru1.lastData + offset1);
#else
        // each config packet occupies 64 * stringsPerPru() bytes per PRU per
        // repeat, so the packet area scales with the chain depth
        m_fv5PacketMem = (uint8_t*)calloc(1, (size_t)128 * 1024 * (m_stringsPerPin / 8));
        setupFalconV5Support(root, m_fv5PacketMem);
#endif
    }

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

// Bit timing, as PRU cycle counts, published where the firmware's per-bit
// waits read them (TIMING_*_OFFSET in BBShiftString.asm - the unused "buffer"
// words, which have to be in the low 256 bytes because LBCO's offset field is
// 8 bit).  The firmware spins on the marker before its first frame, so the
// three counts must be written before it.
//
// The waits count from a clock reset at the start of the bit, and the macro
// spends a few instructions between reading the counter and entering its
// delay loop, so the target is reduced by that overhead here rather than on
// the PRU.  The immediate form compensated 3 (LDI+MAX+SUB); the runtime form
// reads through LBCO instead of LDI, which is slower.
constexpr int TIMING_OVERHEAD_CYCLES = 5;

// ns per PRU cycle: 200MHz on the AM335x, 250MHz on the AM62x
#ifdef PLATFORM_BBB
constexpr int PRU_NS_PER_CYCLE = 5;
#else
constexpr int PRU_NS_PER_CYCLE = 4;
#endif

static uint32_t timingCycles(int ns) {
    int c = ns / PRU_NS_PER_CYCLE - TIMING_OVERHEAD_CYCLES;
    return (uint32_t)std::max(1, c);
}

static void publishTiming(BBBPru* pru, int t0ns, int t1ns, int lowns) {
    volatile uint32_t* p = (volatile uint32_t*)(pru->data_ram + 16);
    p[0] = timingCycles(t0ns);   // TIMING_T0_OFFSET
    p[1] = timingCycles(t1ns);   // TIMING_T1_OFFSET
    p[2] = timingCycles(lowns);  // TIMING_LOW_OFFSET
    __sync_synchronize();
    p[3] = 0xA5A5A5A5;           // TIMING_MAGIC_OFFSET, written last
    __sync_synchronize();
}

// Publish the resolved clock/latch r30 bit numbers to the firmware (see
// PINCFG_OFFSET in BBShiftString.asm; the 0xA5 marker distinguishes a real
// config from cleared memory).  Must land after run() - the firmware load
// clears the data RAM - and the firmware rereads it while waiting for the
// first frame command.
static void publishPinConfig(BBBPru* pru, int clockBit, int latchBit) {
    uint32_t v = (uint32_t)(clockBit & 0xFF) | (((uint32_t)(latchBit & 0xFF)) << 8) | (0xA5u << 24);
    *(volatile uint32_t*)(pru->data_ram + PINCFG_OFFSET) = v;
    __sync_synchronize();
}

// A 16 deep chain shifts twice as many bytes per phase, keeps its output masks
// in the PRU scratchpad and stretches T0H, so it gets its own firmware image
// rather than a runtime switch (see SHIFT16 in BBShiftString.asm).
static std::string pruFirmware(int pru, int stringsPerPin) {
    std::string f = "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString_pru" + std::to_string(pru);
    if (stringsPerPin == 16) {
        f += "_16";
    }
    return f + ".out";
}

int BBShiftStringOutput::StartPRU() {
    m_curFrame = 0;
    m_bpOffered = 0;
    m_bpDeclined = 0;
    m_bpWindowStart = {};
    for (auto& a : m_usedPins) {
        PinCapabilities::getPinByName(a.first).configPin(a.second, true, "BBShiftString");
    }

#ifdef PLATFORM_BBB
    constexpr bool mapShared = false;
#else
    constexpr bool mapShared = true;

    // Where each PRU's data ring sits in the 32KB shared RAM.  Halving it is
    // required when both PRUs output strings, or when a panel driver owns the
    // other half of a combo cape.  A sole string PRU can have the rest,
    // starting above the FalconV5 listener's capture area if a listener will
    // run on the other PRU (the listener captures from the base upwards).
    //
    // 16 deep needs this: it drains the ring at 12.8MB/s, so a 16320 byte half
    // is only 1.27ms of buffer against 1.60ms from the V5 layout (the listener
    // reservation cannot shrink - see SMEMRing.hp).
    uint32_t ringBase[2] = { SMEM_RING_SPLIT0_BASE, SMEM_RING_SPLIT1_BASE };
    uint32_t ringSize[2] = { SMEM_RING_SPLIT_SIZE, SMEM_RING_SPLIT_SIZE };
    const bool bothPrus = m_pru0.maxStringLen && m_pru1.maxStringLen;
    const bool willListen = supportsV5Listeners && m_hasBidirSR;
    if (m_stringsPerPin == 16) {
        if (bothPrus || m_sharedPRUSS) {
            // 16 deep already fills a PRU's pins; needing both means the cape
            // is asking for something the shared RAM cannot feed.
            LogErr(VB_CHANNELOUT, "BBShiftString: 16 strings per pin needs sole use of one PRU%s\n",
                   m_sharedPRUSS ? " and cannot share the PRUSS with a panel driver" : "");
            WarningHolder::AddWarning("BBShiftString: 16 strings per pin requires all strings on one PRU");
        } else {
            int p = m_pru1.maxStringLen ? 1 : 0;
            if (willListen && p == 0) {
                // the listener is a PRU0 program; strings must leave it free
                LogErr(VB_CHANNELOUT, "BBShiftString: FalconV5 listeners need PRU0, so the strings must be on PRU1\n");
                WarningHolder::AddWarning("BBShiftString: FalconV5 listeners require the strings on PRU1");
            }
            ringBase[p] = willListen ? SMEM_RING_V5_BASE : SMEM_RING_DEFAULT_BASE;
            ringSize[p] = willListen ? SMEM_RING_V5_SIZE : SMEM_RING_DEFAULT_SIZE;
        }
    }
    LogDebug(VB_CHANNELOUT, "BBShiftString: ring pru0 %X/%d pru1 %X/%d (listener %d)\n",
             ringBase[0], ringSize[0], ringBase[1], ringSize[1], willListen ? 1 : 0);
#endif
    if (m_pru1.maxStringLen) {
        m_pru1.pru = new BBBPru(1, mapShared, false);
        m_pru1.pruData = (BBShiftStringData*)m_pru1.pru->data_ram;
        if (!m_pru1.pru->run(pruFirmware(1, m_stringsPerPin), !m_sharedPRUSS)) {
            LogErr(VB_CHANNELOUT, "BBShiftString: Unable to start PRU1. May require a reboot.\n");
            WarningHolder::AddWarning("BBShiftString: Unable to start PRU1. May require a reboot.");
            return 0;
        }
        publishPinConfig(m_pru1.pru, m_clockBit[1], m_latchBit[1]);
        publishTiming(m_pru1.pru, m_t0Ns, m_t1Ns, m_lowNs);
#ifndef PLATFORM_BBB
        // the firmware polls for the ring location; the upper half of the
        // shared RAM keeps clear of the FalconV5 listener capture area
        m_pru1.ring.attach(m_pru1.pru, ringBase[1], ringSize[1], true);
#endif
        createOutputLengths(m_pru1, "pru1");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (m_pru0.maxStringLen) {
        m_pru0.pru = new BBBPru(0, mapShared, false);
        m_pru0.pruData = (BBShiftStringData*)m_pru0.pru->data_ram;
        if (!m_pru0.pru->run(pruFirmware(0, m_stringsPerPin), !m_sharedPRUSS)) {
            LogErr(VB_CHANNELOUT, "BBShiftString: Unable to start PRU0. May require a reboot.\n");
            WarningHolder::AddWarning("BBShiftString: Unable to start PRU0. May require a reboot.");
            return 0;
        }
        publishPinConfig(m_pru0.pru, m_clockBit[0], m_latchBit[0]);
        publishTiming(m_pru0.pru, m_t0Ns, m_t1Ns, m_lowNs);
#ifndef PLATFORM_BBB
        m_pru0.ring.attach(m_pru0.pru, ringBase[0], ringSize[0], true);
#endif
        createOutputLengths(m_pru0, "pru0");
    }
#ifndef PLATFORM_BBB
    if (!m_pumpThread.joinable() && (m_pru0.pru || m_pru1.pru)) {
        m_pumpRunning = true;
        m_pumpThread = std::thread(&BBShiftStringOutput::runPumpThread, this);
    }
#endif
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
    }
    if (m_pru0.pru) {
        int cnt = 0;
        while (wait && cnt < 25 && m_pru0.pruData->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
            __asm__ __volatile__("" ::
                                     : "memory");
        }
    }
    // stop the pump before the PRU memory mappings go away
    m_pumpRunning = false;
    if (m_pumpThread.joinable()) {
        m_pumpThread.join();
    }

    if (m_pru1.pru) {
        m_pru1.pru->stop();
        delete m_pru1.pru;
        m_pru1.pru = nullptr;
    }

    if (m_pru0.pru) {
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
    setFrameRateWarning(false);
    clearBudgetWarning();
    for (auto& a : m_usedPins) {
        PinCapabilities::getPinByName(a.first).releasePin();
    }
    if (supportsV5Listeners && m_hasBidirSR) {
        PinCapabilities::getPinByName(PRU1_ENABLE_PIN).releasePin();
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

// 8x8 byte transpose: in, rows are 8 consecutive bytes of 8 strings; out,
// rows are the 8 strings' bytes for 8 consecutive positions
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

// Transpose one 8x8 block of the interleaved frame: eight source bytes, one
// per data pin, taken inStride apart, become eight bit planes outStride apart.
// SH selects which bit of the pixel byte this plane carries; the caller emits
// SH 0 (the LSB, clocked out last) into the highest numbered plane.  Within a
// plane, lane i is the byte destined for shift register stage i.
//
// The reference implementation this replaces, per pixel byte:
//     for (int x = 0; x < 8; x++) {
//         iframe[7-x] = (buf[0] >> x) & 0x0101010101010101ULL;
//         iframe[7-x] |= ((buf[1] >> x) & mask) << 1;
//         ... one term per data pin ...
//     }
// On 32 bit arm each of those 64 bit shifts is three instructions, hence NEON.
template<int SH>
static inline void bitFlipPlane(const uint8x8_t* buf, uint8_t* out) {
    uint8x8_t b[8];
    // vshr_n_u8 requires 1 <= n <= 8, so the LSB plane takes the bytes as they
    // are; vsli_n_u8 preserves the low bits of its first operand either way
    if constexpr (SH == 0) {
        for (int k = 0; k < 8; ++k) {
            b[k] = buf[k];
        }
    } else {
        for (int k = 0; k < 8; ++k) {
            b[k] = vshr_n_u8(buf[k], SH);
        }
    }
    uint8x8_t tmp = vsli_n_u8(b[0], b[1], 1);
    tmp = vsli_n_u8(tmp, b[2], 2);
    tmp = vsli_n_u8(tmp, b[3], 3);
    tmp = vsli_n_u8(tmp, b[4], 4);
    tmp = vsli_n_u8(tmp, b[5], 5);
    tmp = vsli_n_u8(tmp, b[6], 6);
    vst1_u8(out, vsli_n_u8(tmp, b[7], 7));
}

template<int SPP>
void BBShiftStringOutput::bitFlipDataT(uint8_t* stringChannelData, uint8_t* bitSwapped, size_t len) {
    constexpr int NSTR = MAX_PINS_PER_PRU * SPP;
    uint8_t* iframe = bitSwapped;
    for (size_t p = 0; p < len; p++) {
        // one 8x8 transpose per group of eight shift register stages; a 16
        // deep chain is just two of them, landing in the two halves of each
        // plane
        for (int h = 0; h < SPP / 8; ++h) {
            const int off = h * 8;
            uint8x8_t buf[8];
            for (int k = 0; k < 8; ++k) {
                buf[k] = vld1_u8(&stringChannelData[k * SPP + off]);
            }
            bitFlipPlane<0>(buf, &iframe[7 * SPP + off]);
            bitFlipPlane<1>(buf, &iframe[6 * SPP + off]);
            bitFlipPlane<2>(buf, &iframe[5 * SPP + off]);
            bitFlipPlane<3>(buf, &iframe[4 * SPP + off]);
            bitFlipPlane<4>(buf, &iframe[3 * SPP + off]);
            bitFlipPlane<5>(buf, &iframe[2 * SPP + off]);
            bitFlipPlane<6>(buf, &iframe[1 * SPP + off]);
            bitFlipPlane<7>(buf, &iframe[0 * SPP + off]);
        }
        iframe += NSTR;
        stringChannelData += NSTR;
    }
}

void BBShiftStringOutput::bitFlipData(uint8_t* stringChannelData, uint8_t* bitSwapped, size_t len) {
    if (m_stringsPerPin == 16) {
        bitFlipDataT<16>(stringChannelData, bitSwapped, len);
    } else {
        bitFlipDataT<8>(stringChannelData, bitSwapped, len);
    }
}

template<int SPP>
void BBShiftStringOutput::prepDataT(FrameData& d, unsigned char* channelData) {
    if (d.maxStringLen == 0) {
        return;
    }
    PixelStringTester* tester = nullptr;
    if (m_testType && m_testCycle >= 0) {
        if (m_testType == 999 && falconV5Support && m_testCycle == 0) {
            falconV5Support->sendCountPixelPackets();
        }
        tester = PixelStringTester::getPixelStringTester(m_testType);
        tester->prepareTestData(m_testCycle, m_testPercent);
    }
    constexpr int NSTR = MAX_PINS_PER_PRU * SPP;
    // per string slot: either a fully prepared buffer (test mode) or the
    // PixelString whose virtual strings are rendered on the fly per tile,
    // with a cursor tracking where in the virtual string list the next tile
    // continues
    struct SlotSrc {
        const uint8_t* buf = nullptr;
        PixelString* ps = nullptr;
        const uint8_t* affine = nullptr;
        uint32_t len = 0;
        uint32_t vsIdx = 0;
        uint32_t vsOff = 0;
        uint8_t pad = 0;
    } slots[MAX_PINS_PER_PRU][SPP];
    uint32_t newMax = d.maxStringLen;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        for (int x = 0; x < SPP; ++x) {
            int idx = d.stringMap[y][x];
            if (idx != -1) {
                PixelString* ps = m_strings[idx];
                uint32_t newLen = ps->m_outputBytes;
                SlotSrc& sl = slots[y][x];
                if (tester) {
                    sl.buf = tester->createTestData(ps, m_testCycle, m_testPercent, channelData, newLen);
                } else {
                    sl.ps = ps;
                    sl.affine = m_vsAffine[idx].data();
                }
                sl.len = newLen;
                // Past the end of a string the port is parked at its idle
                // level, which an inverted line holds high.  The data phase
                // still shifts whatever is in the buffer, so padding a short
                // inverted port with zeros would drop the line mid bit.
                sl.pad = ps->m_isInverted ? 0xFF : 0x00;
                newMax = std::max(newMax, newLen);
            }
        }
    }
    d.outputStringLen = newMax;

    // Interleave and bit flip the string data in tiles that stay in the L1
    // cache.  Interleaving the whole frame at once writes a byte every 64
    // bytes which forces every cache line of the frame to be re-fetched for
    // every string once the frame no longer fits in cache.  The brightness/
    // gamma application is fused into the tile fill so the string bytes are
    // never staged in a full-size intermediate buffer, and virtual strings
    // whose channel map is a simple run (map[i+3] == map[i]+3, the normal
    // non-grouped case) skip the per-channel map indirection.
    constexpr uint32_t TILE = 64;
    uint8_t col[SPP][TILE];
    uint8_t tile[TILE * NSTR];
    for (uint32_t p0 = 0; p0 < newMax; p0 += TILE) {
        const uint32_t n = std::min(TILE, newMax - p0);
        const uint32_t nFull = n & ~7;
        for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
            for (int x = 0; x < SPP; ++x) {
                SlotSrc& sl = slots[y][x];
                uint32_t avail = sl.len > p0 ? std::min(n, sl.len - p0) : 0;
                uint32_t p = 0;
                if (sl.buf) {
                    memcpy(col[x], sl.buf + p0, avail);
                    p = avail;
                } else if (sl.ps) {
                    // A protocol preamble (TM1814's C1/C2 current words) is
                    // constant and leads the port's stream, so it is consumed
                    // out of the first tile before the virtual strings start.
                    const auto& pre = sl.ps->m_preamble;
                    while (p < avail && (p0 + p) < pre.size()) {
                        col[x][p] = pre[p0 + p];
                        ++p;
                    }
                    auto& vstrings = sl.ps->m_virtualStrings;
                    if (sl.ps->m_bytesPerChannel == 2) {
                        // 16 bit part: two bytes per channel, most significant
                        // first.  vsOff counts wire bytes here rather than
                        // channels, so the channel index is vsOff >> 1.  Kept
                        // as its own loop so the 8 bit path below is untouched.
                        while (p < avail && sl.vsIdx < vstrings.size()) {
                            auto& vs = vstrings[sl.vsIdx];
                            uint32_t vsBytes = (uint32_t)vs.chMapCount * 2;
                            uint32_t m = std::min(avail - p, vsBytes - sl.vsOff);
                            const uint16_t* br = vs.brightnessMap16;
                            const int* mp = vs.chMap;
                            for (uint32_t k = 0; k < m; ++k) {
                                uint32_t wb = sl.vsOff + k;
                                uint16_t v = br[channelData[mp[wb >> 1]]];
                                col[x][p++] = (wb & 1) ? (uint8_t)v : (uint8_t)(v >> 8);
                            }
                            sl.vsOff += m;
                            if (sl.vsOff >= vsBytes) {
                                sl.vsOff = 0;
                                sl.vsIdx++;
                            }
                        }
                        if (p < n) {
                            memset(&col[x][p], sl.pad, n - p);
                        }
                        continue;
                    }
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
                    memset(&col[x][p], sl.pad, n - p);
                }
            }
            uint32_t g = 0;
            for (; g < nFull; g += 8) {
                // transpose8x8 handles eight strings at a time, so a 16 deep
                // chain needs one pass per half, each landing in its own half
                // of the pin's slot range
                for (int h = 0; h < SPP / 8; ++h) {
                    uint8x8_t r[8];
                    for (int x = 0; x < 8; ++x) {
                        r[x] = vld1_u8(&col[h * 8 + x][g]);
                    }
                    transpose8x8(r);
                    for (int i = 0; i < 8; i++) {
                        vst1_u8(&tile[(g + i) * NSTR + y * SPP + h * 8], r[i]);
                    }
                }
            }
            for (uint32_t p = g; p < n; ++p) {
                for (int x = 0; x < SPP; ++x) {
                    tile[p * NSTR + y * SPP + x] = col[x][p];
                }
            }
        }
        bitFlipDataT<SPP>(tile, d.formattedData + (size_t)p0 * NSTR, n);
    }
}

// The firmware counts the frames it had to re-seat onto the published frame
// start (see CONT_DATA in BBShiftString.asm).  In a healthy system this stays
// zero for the life of the output; anything else means the two sides disagreed
// about a frame's length and the resync covered for it, at the cost of one
// glitched frame.
void BBShiftStringOutput::reportRingResync(FrameData& d, int pru) {
#ifndef PLATFORM_BBB
    if (!d.pruData) {
        return;
    }
    uint32_t n = *(volatile uint32_t*)((uint8_t*)d.pruData + SMEM_RING_RESYNC_OFFSET);
    if (n != d.resyncCount) {
        LogWarn(VB_CHANNELOUT, "BBShiftString: PRU%d ring read position re-seated %u time(s) (%u total); a frame was glitched but the output is aligned\n",
                pru, n - d.resyncCount, n);
        d.resyncCount = n;
    }
#endif
}

void BBShiftStringOutput::prepData(FrameData& d, unsigned char* channelData) {
    if (m_stringsPerPin == 16) {
        prepDataT<16>(d, channelData);
    } else {
        prepDataT<8>(d, channelData);
    }
}

void BBShiftStringOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftStringOutput::PrepData(%p)\n", channelData);
    if (!hasStrings()) {
        return;
    }
    // auto start = std::chrono::high_resolution_clock::now();
    m_curFrame++;

    // the firmware heals a ring misalignment on its own, but it should never
    // have to - surface it rather than let it stay silent
    if ((m_curFrame % 100) == 0) {
        reportRingResync(m_pru0, 0);
        reportRingResync(m_pru1, 1);
    }

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
#ifdef PLATFORM_BBB
        // only the bytes the PRU will consume need to be copied/flushed
        uint32_t bytes = d.outputStringLen * stringsPerPru();
        if (bytes > d.frameSize) {
            bytes = d.frameSize;
        }
        memcpy(d.curData, d.formattedData, bytes);
        // make sure memory is flushed
        msync(d.curData, bytes, MS_SYNC | MS_INVALIDATE);
        __builtin___clear_cache(d.curData, d.curData + bytes);

        d.pruData->address_dma = (d.pru->ddr_addr + (d.curData - d.pru->ddr));
        if (d.v5_config_packets[d.curV5ConfigPacket]) {
            d.pruData->address_dma_packet = (d.pru->ddr_addr + (d.v5_config_packets[d.curV5ConfigPacket]->data - d.pru->ddr));
        } else {
            d.pruData->address_dma_packet = 0;
        }
        std::swap(d.lastData, d.curData);
#else
        // the pump thread streams the frame (and any FalconV5 packet data)
        // into the shared memory ring in exactly the order the firmware
        // consumes blocks; the PRU consumes one 64 byte block per byte of
        // string data
        uint32_t bytes = d.outputStringLen * stringsPerPru();
        if (bytes > d.frameSize) {
            bytes = d.frameSize;
        }
        memcpy(d.curData, d.formattedData, bytes);
        d.pendingFrame.frameData = d.curData;
        d.pendingFrame.frameBytes = bytes;
        auto* pi = d.v5_config_packets[d.curV5ConfigPacket];
        if (pi && pi->data) {
            d.pendingFrame.packetData = pi->data;
            d.pendingFrame.packetBytes = 57 * stringsPerPru() * pi->len;
        } else {
            d.pendingFrame.packetData = nullptr;
            d.pendingFrame.packetBytes = 0;
        }
        std::swap(d.lastData, d.curData);
#endif
    }
}

// Persistent UI warning for "the sequence rate is past what these strings can
// clock out".  The flag is per output so Close() can retract it; a warning left
// behind by an output that no longer exists has nothing left to clear it.
void BBShiftStringOutput::setFrameRateWarning(bool on) {
    static const std::string BP_WARN =
        "Sequence frame rate is higher than the configured pixel strings can output; frames are being dropped";
    if (on == m_bpWarned) {
        return;
    }
    if (on) {
        WarningHolder::AddWarning(61, BP_WARN);
    } else {
        WarningHolder::RemoveWarning(61, BP_WARN);
    }
    m_bpWarned = on;
}

// Predictive companion to the measured warning above, run whenever the
// refresh rate changes.  Sequence.cpp publishes the sequence's rate before the
// first frame reaches SendData(), so an over-budget configuration is flagged
// immediately instead of after a minute of measured drops - and a rate that
// changes mid-playlist (sequences at different frame rates back to back, where
// the output thread never restarts) is caught on the next frame.
void BBShiftStringOutput::checkFrameRateBudget(float rate) {
    if (m_frameTimeUs <= 0) {
        return;
    }
    float ceiling = 1000000.0f / m_frameTimeUs;
    // 1% grace: a hair past the budget is absorbed by the back-pressure gate
    // at a drop rate nobody will see, and rounding noise must not warn
    if (rate > ceiling * 1.01f) {
        char buf[192];
        snprintf(buf, sizeof(buf),
                 "Sequence frame rate (%.4g fps) is higher than the configured pixel strings can output (~%.4g fps); frames will be dropped",
                 rate, ceiling);
        std::string msg = buf;
        if (msg != m_budgetWarnText) {
            clearBudgetWarning();
            LogWarn(VB_CHANNELOUT, "BBShiftString: %s\n", msg.c_str());
            WarningHolder::AddWarning(63, msg);
            m_budgetWarnText = msg;
        }
    } else {
        clearBudgetWarning();
    }
}

void BBShiftStringOutput::clearBudgetWarning() {
    if (!m_budgetWarnText.empty()) {
        WarningHolder::RemoveWarning(63, m_budgetWarnText);
        m_budgetWarnText.clear();
    }
}

int BBShiftStringOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftStringOutput::SendData(%p)\n", channelData);

    float bwRate = GetChannelOutputRefreshRate();
    if (bwRate != m_lastBudgetRate) {
        m_lastBudgetRate = bwRate;
        checkFrameRateBudget(bwRate);
    }
    if (!hasStrings()) {
        return 0;
    }

    if (falconV5Support) {
        falconV5Support->processListenerData();
    }

#ifndef PLATFORM_BBB
    // ---- back-pressure gate --------------------------------------------------
    // pumpFrameData() only snapshots pendingFrame once the firmware is ready for
    // another frame (command == 0 && ring drained).  A frame staged before that
    // point is simply overwritten and never rendered.  When the requested frame
    // rate exceeds what the configured string length can physically clock out,
    // that surplus of never-rendered frames is what eventually leaves the PRU
    // ring misaligned (#2855).  Declining the frame while the previous one is
    // still pending drives the surplus to zero at whatever rate the hardware
    // actually sustains - frame dropping at the source, with no fixed divisor
    // to mis-tune, and configurations already inside their budget are untouched
    // (nothing is pending when the next frame arrives, so nothing is declined).
    //
    // It is also what keeps the two-phase write of pendingFrame safe: sendData()
    // fills the byte counts and the block below fills the command, and only a
    // frame the pump has already taken may be restaged, so the pump can never
    // snapshot one phase of one frame with the other phase of the next.  Ditto
    // the FalconV5 packet cursor - curV5ConfigPacket only advances for a frame
    // that will actually be streamed, where before, over budget, better than
    // half the receiver's config and dynamic packets were marked consumed and
    // never sent.
    //
    // AM335x never reaches this: it has no ring and no pump, every frame
    // restates its own DDR address and length, and pendingSeq is never bumped.
    //
    // Cannot wedge: we only decline while pendingSeq != pumpedSeq, so
    // pumpFrameData() still enters its busy branch and its 250ms watchdog
    // still runs.  A PRU is only party to the gate if the pump actually
    // services it (pump thread condition below) - otherwise its pendingSeq
    // would climb against a pumpedSeq nothing advances and every frame,
    // including the other PRU's, would be declined forever.
    //
    // A declined frame is not retried, so a one-shot frame can land late: the
    // end-of-sequence blank is offered three times (the forced output, then
    // twice more as onceMore unwinds), the last of those BridgeLightDelay
    // (E131BridgingInterval, 50ms default) after the one before it, which is
    // past the drain time of any frame this gate declines.  Set that interval
    // below a frame's clocking time and blanking would be delayed further.
    {
        bool taken = true;
        if (m_pru1.pru && m_pru1.ring.attached()) {
            taken = taken && (m_pru1.pendingSeq.load(std::memory_order_acquire) ==
                              m_pru1.pumpedSeq.load(std::memory_order_acquire));
        }
        if (m_pru0.pru && m_pru0.ring.attached()) {
            taken = taken && (m_pru0.pendingSeq.load(std::memory_order_acquire) ==
                              m_pru0.pumpedSeq.load(std::memory_order_acquire));
        }
        ++m_bpOffered;
        if (!taken) {
            ++m_bpDeclined;
        }
        // report on a wall-clock window rather than a frame count so the
        // hysteresis reacts at the same speed whatever the sequence rate
        auto now = std::chrono::steady_clock::now();
        if (m_bpWindowStart.time_since_epoch().count() == 0) {
            m_bpWindowStart = now;
        } else if ((now - m_bpWindowStart) >= std::chrono::seconds(60)) {
            if (m_bpDeclined) {
                LogWarn(VB_CHANNELOUT,
                        "BBShiftString: back-pressure gate declined %u of %u frames (%.1f%%)\n",
                        m_bpDeclined, m_bpOffered, 100.0 * m_bpDeclined / m_bpOffered);
            } else {
                LogInfo(VB_CHANNELOUT, "BBShiftString: back-pressure gate declined no frames of %u\n",
                        m_bpOffered);
            }
            // Surface a persistent UI warning once the sequence rate is clearly
            // beyond what the strings can output; clear it again when the rate
            // drops back (e.g. a different sequence starts).  The thresholds
            // are hysteresis: on at >=10% dropped, off again below 2%.
            if (m_bpDeclined * 10 >= m_bpOffered) {
                setFrameRateWarning(true);
            } else if (m_bpDeclined * 50 <= m_bpOffered) {
                setFrameRateWarning(false);
            }
            m_bpOffered = 0;
            m_bpDeclined = 0;
            m_bpWindowStart = now;
        }
        if (!taken) {
            return m_channelCount;
        }
    }
    // --------------------------------------------------------------------------
#endif

    sendData(m_pru0);
    sendData(m_pru1);
    // make sure memory is flushed before command is set to 1
    __asm__ __volatile__("" ::
                             : "memory");

    // Send the start command.  A port set with no data to send must not get a
    // command at all: sendData() above leaves pendingFrame empty in that case,
    // so the command would name a frame the pump never streams.  The length is
    // also not enough on its own to make it a no-op - a zero length still ORs
    // in the custom-length flag below, and the firmware's render loop reads its
    // first block before testing the count, so a "zero byte" frame still eats a
    // ring block the ARM never wrote.  That block offsets every later frame by
    // one byte per string for as long as the output lives, and because both
    // sides still move the same number of blocks per frame the ring keeps
    // reaching empty and drained() never notices.  A freshly Init()ed output
    // hits this on a config reload: PrepareChannelData() and SendChannelData()
    // are separate walks of the output list, so the replacement can pick up its
    // first SendData() before its first PrepData() has set outputStringLen.
    if (m_pru0.pruData && m_pru0.outputStringLen) {
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
#ifdef PLATFORM_BBB
        m_pru0.pruData->command = c;
#else
        // the pump thread writes the command once the PRU has taken the
        // previous one; the gate above guarantees the previous one is gone,
        // so this can never overwrite a frame the pump has yet to snapshot
        m_pru0.pendingFrame.command = c;
        m_pru0.pendingSeq.fetch_add(1, std::memory_order_release);
#endif
    }
    if (m_pru1.pruData && m_pru1.outputStringLen) {
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
#ifdef PLATFORM_BBB
        m_pru1.pruData->command = c;
#else
        m_pru1.pendingFrame.command = c;
        m_pru1.pendingSeq.fetch_add(1, std::memory_order_release);
#endif
    }
    __asm__ __volatile__("" ::
                             : "memory");

    return m_channelCount;
}

#ifndef PLATFORM_BBB
// Stream the pending frame into the shared memory ring; returns true if any
// progress was made.  The full frame does not fit in the ring, so the PRU is
// started as soon as a frame is taken and the data streams ~2.5ms ahead of
// the output.
bool BBShiftStringOutput::pumpFrameData(FrameData& d) {
    if (!d.pumpActive) {
        uint32_t seq = d.pendingSeq.load(std::memory_order_acquire);
        // The command word says how many blocks to render but not WHERE the
        // frame starts - the firmware just reads on from wherever it stopped.
        // So a command may only be issued when the ring is empty, which is the
        // only moment the read pointer provably sits on this frame's first
        // byte.  Without this the two channels drift apart by whatever was
        // still buffered when the first command went out, and every frame
        // after that renders that many bytes into the previous one.  Streaming
        // ahead within a frame is unaffected (that is the loop below).
        if (seq == d.pumpedSeq.load(std::memory_order_relaxed)) {
            return false;
        }
        if (d.pruData->command != 0 || !d.ring.drained()) {
            // The firmware has not finished with what it was already given.
            // That clears inside a frame time in normal operation; if it does
            // not, it is starved on a frame whose data never arrived (or on a
            // command that went missing), and waiting on the ring alone would
            // leave the output dead for good.  Re-seat on its read pointer and
            // carry on rather than wedge.
            uint32_t rendered = *(volatile uint32_t*)((uint8_t*)d.pruData + SMEM_RING_FRAMES_OFFSET);
            auto now = std::chrono::steady_clock::now();
            if (rendered != d.renderedFrames || d.stalledSince.time_since_epoch().count() == 0) {
                d.renderedFrames = rendered;
                d.stalledSince = now;
            } else if ((now - d.stalledSince) > std::chrono::milliseconds(250)) {
                LogWarn(VB_CHANNELOUT, "BBShiftString: firmware rendered no frame for 250ms (command 0x%X, %u bytes unread); resynchronizing the ring\n",
                        (unsigned)d.pruData->command, d.ring.usedBytes());
                d.pruData->command = 0;
                d.ring.seekToConsumer();
                d.stalledSince = now;
            }
            return false;
        }
        d.stalledSince = {};
        // Snapshot the newest pending frame; retry if SendData raced us.
        // pumpedSeq is published only once the copy is known good, never
        // before it: the back-pressure gate in SendData() treats
        // pumpedSeq == pendingSeq as its licence to restage, so publishing
        // first would invite the output thread to rewrite pendingFrame while
        // this copy is still reading it.
        while (true) {
            uint32_t snapped = seq;
            d.activeFrame = d.pendingFrame;
            seq = d.pendingSeq.load(std::memory_order_acquire);
            if (seq == snapped) {
                d.pumpedSeq.store(snapped, std::memory_order_release);
                break;
            }
        }
        d.activeOff = 0;
        d.pumpActive = true;
        // Publish where in the ring this frame starts before the command that
        // makes the firmware act on it - nothing has been written for this
        // frame yet, so the write position is its first byte.  The firmware
        // takes its read position from this and zeroes it, so the two only
        // ever move together.  This is what keeps a one-off disagreement about
        // a frame's length from becoming permanent: the drain gate above
        // cannot catch that on its own, because a frame offset by a whole
        // number of blocks empties the ring exactly like one offset by none.
        d.pruData->address_dma = d.ring.writePos();
        __sync_synchronize();
        d.pruData->command = d.activeFrame.command;
    }
    uint32_t total = d.activeFrame.frameBytes + d.activeFrame.packetBytes;
    bool progress = false;
    while (d.activeOff < total) {
        const uint8_t* src;
        uint32_t remaining;
        if (d.activeOff < d.activeFrame.frameBytes) {
            src = d.activeFrame.frameData + d.activeOff;
            remaining = d.activeFrame.frameBytes - d.activeOff;
        } else {
            src = d.activeFrame.packetData + (d.activeOff - d.activeFrame.frameBytes);
            remaining = total - d.activeOff;
        }
        uint32_t n = d.ring.write(src, remaining);
        if (n == 0) {
            break;
        }
        d.activeOff += n;
        progress = true;
    }
    if (d.activeOff == total) {
        d.pumpActive = false;
    }
    return progress;
}

void BBShiftStringOutput::runPumpThread() {
    struct sched_param sp;
    sp.sched_priority = 50;
    int err = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (err) {
        LogWarn(VB_CHANNELOUT, "BBShiftString: could not set pump thread to SCHED_FIFO: %s\n", strerror(err));
    }
    while (m_pumpRunning) {
        bool p = false;
        if (m_pru0.pru && m_pru0.ring.attached()) {
            p |= pumpFrameData(m_pru0);
        }
        if (m_pru1.pru && m_pru1.ring.attached()) {
            p |= pumpFrameData(m_pru1);
        }
        if (!p) {
            struct timespec ts = { 0, 500000 };
            nanosleep(&ts, nullptr);
        }
    }
}
#endif

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
// No inversion pass here: config packets only exist for a chain's first port,
// and demoteInvertedReceiverChains() has already dropped any chain whose head
// is inverted, so a packet never needs flipping.
void BBShiftStringOutput::encodeFalconV5Packet(std::vector<std::array<uint8_t, 64>>& packets, uint8_t* memLocPru0, uint8_t* memLocPru1) {
    std::array<uint8_t, 57 * MAX_PINS_PER_PRU * MAX_STRINGS_PER_PIN> pru0Data;
    std::array<uint8_t, 57 * MAX_PINS_PER_PRU * MAX_STRINGS_PER_PIN> pru1Data;
    for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
        uint8_t pinMask = 1 << y;
        for (int x = 0; x < m_stringsPerPin; ++x) {
            int idx = m_pru0.stringMap[y][x];
            if (idx != -1) {
                uint8_t* frame = &pru0Data[x + (y * m_stringsPerPin)];
                for (int p = 0; p < 57; p++) {
                    uint8_t b = packets[idx][p];
                    b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
                    *frame = b;
                    frame += stringsPerPru();
                }
            }
            idx = m_pru1.stringMap[y][x];
            if (idx != -1) {
                uint8_t* frame = &pru1Data[x + (y * m_stringsPerPin)];
                for (int p = 0; p < 57; p++) {
                    uint8_t b = packets[idx][p];
                    b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
                    *frame = b;
                    frame += stringsPerPru();
                }
            }
        }
    }
    size_t pLen = 57 * stringsPerPru();
    if (m_pru0.maxStringLen) {
        bitFlipData(&pru0Data[0], memLocPru0, 57);
    }
    if (m_pru1.maxStringLen) {
        bitFlipData(&pru1Data[0], memLocPru1, 57);
    }
}

void BBShiftStringOutput::setupFalconV5Support(const Json::Value& root, uint8_t* memLoc) {
    falconV5Support = new FalconV5Support();
    if (supportsV5Listeners && m_hasBidirSR) {
        falconV5Support->addListeners(root["falconV5ListenerConfig"]);
    }

    int max = std::max(m_pru0.maxStringLen, m_pru1.maxStringLen);
    int x = 0;
    while (x < m_strings.size()) {
        int p = x;
        PixelString* p1 = m_strings[x++];
        if (p1->m_isSmartReceiver && (p1->smartReceiverType == PixelString::ReceiverType::FalconV5 || p1->smartReceiverType == PixelString::ReceiverType::FalconV4)) {
            // V4 chains are send-only: the config packet goes out, but no
            // queries are scheduled and no responses are listened for
            bool sendOnly = p1->smartReceiverType == PixelString::ReceiverType::FalconV4;
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

            falconV5Support->addReceiverChain(p1, p2, p3, p4, grp, mux, sendOnly);
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
            int len = (x == 1) ? 2 : 1;
            int idx = x;
            bool listen = false;
            bool any = false;
            for (auto& rc : falconV5Support->getReceiverChains()) {
                int port = rc->getPixelStrings()[0]->m_portNumber;
                if (x > 0 && rc->isSendOnly()) {
                    // V4 receivers only get the config packet
                    continue;
                }
                any = true;
                if (x == 0) {
                    rc->generateConfigPacket(&packets[port][0]);
                } else if (x == 1) {
                    rc->generateNumberPackets(&packets[port][0], &packets2[port][0]);
                } else if (x == 2) {
                    rc->generateSetFusesPacket(&packets[port][0], 1);
                }
            }
            if (!any) {
                continue;
            }
            if (m_pru0.v5_config_packets[idx] == nullptr) {
                m_pru0.v5_config_packets[idx] = new FalconV5PacketInfo(len, memLoc, listen);
                // 64 to keep on 4K memory alignment
                memLoc += 64 * stringsPerPru() * len;
                m_pru1.v5_config_packets[idx] = new FalconV5PacketInfo(len, memLoc, listen);
                memLoc += 64 * stringsPerPru() * len;

                encodeFalconV5Packet(packets, m_pru0.v5_config_packets[idx]->data, m_pru1.v5_config_packets[idx]->data);
                if (len == 2) {
                    size_t pLen = 57 * stringsPerPru();
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
    // Reserve space for the dynamic packets.  The len must be set as well:
    // the AM62x pump streams 57*64*len packet bytes into the ring, and a
    // dynamic packet whose command flags said "packet present" but whose
    // len was 0 starves the PRU in its ring wait (the AM335x path reads by
    // DDR address and never noticed).
    m_pru0.dynamicPacketInfo1.data = memLoc;
    m_pru0.dynamicPacketInfo1.len = 1;
    memLoc += 64 * stringsPerPru();
    m_pru0.dynamicPacketInfo2.data = memLoc;
    m_pru0.dynamicPacketInfo2.len = 1;
    memLoc += 64 * stringsPerPru();
    m_pru1.dynamicPacketInfo1.data = memLoc;
    m_pru1.dynamicPacketInfo1.len = 1;
    memLoc += 64 * stringsPerPru();
    m_pru1.dynamicPacketInfo2.data = memLoc;
    m_pru1.dynamicPacketInfo2.len = 1;
    memLoc += 64 * stringsPerPru();

    m_pru0.dynamicPacketInfo = &m_pru0.dynamicPacketInfo1;
    m_pru1.dynamicPacketInfo = &m_pru1.dynamicPacketInfo1;

    createOutputLengths(m_pru0, "pru0");
    createOutputLengths(m_pru1, "pru1");
}
