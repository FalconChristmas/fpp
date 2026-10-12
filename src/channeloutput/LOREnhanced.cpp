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

#include "fpp-json.h"

#include <cstdint>
#include <vector>

#include "../Warnings.h"
#include "../common.h"
#include "../log.h"

#include "LOREnhanced.h"
#include "LORSerial.h"

// A Pixie port set for 340 pixels takes two unit IDs, so a unit ID never
// carries more than 170 pixels.
#define LORE_MAX_PIXELS_PER_UNIT 170
#define LORE_MAX_CHANNELS_PER_UNIT (LORE_MAX_PIXELS_PER_UNIT * 3)
#define LOR_ENHANCED_CMD 0x8E

/*
 * Enhanced command: 00 <unit> 8E <ops...> 00
 *
 * The ops walk the unit's channels from channel 1.  Each op is a command
 * byte, [7:5] the op and [4:0] a count; a count of 11111 means the count is
 * in the next byte instead (FPP only uses that for counts over 30).
 *
 *   000  literal: the next <count> bytes are values for the next channels
 *   001  jump: skip <count> channels
 *   010  repeat: the next 3 bytes are one pixel, written to <count> pixels
 *
 * Values are 1-255 (0 is the delimiter, so 0 is sent as 1).  Checked on a
 * Pixie: a jump leaves the channels it skips unchanged, and so do channels
 * past the end of the packet, so a frame can carry only what changed.  A
 * unit blanks its outputs when the heartbeat stops.
 */

/////////////////////////////////////////////////////////////////////////////

class LOREnhancedOutputUnit {
public:
    int unitId = 1;
    int numOfPixels = 0;
    int lorStartPixel = 1;
    int fppStartAddr = 0;

    // What the unit was last sent for each of its channels, or LOR_UNKNOWN.
    std::vector<uint16_t> lastSent;
};

class LOREnhancedOutputData : public LORSerialOutput {
public:
    LOREnhancedOutputData() {
        speed = 500000;
    }

    std::vector<LOREnhancedOutputUnit> units;

    // Unit the next frame starts with, so a frame whose changes do not all
    // fit the budget does not starve the units after it.
    int nextUnit = 0;
    // Next pixel the background refresh marks for resending.
    int refreshUnit = 0;
    int refreshPixel = 0;

    unsigned char batch[LOR_MAX_BATCH_SIZE];

    // One per packet in the batch: the unit, the pixels its ops reach, and
    // where the packet ends in the batch, so a short write can mark them
    // unsent again.
    struct SentPacket {
        int unit;
        int firstPixel;
        int endPixel;
        int batchEnd;
    };
    std::vector<SentPacket> sent;
};

/////////////////////////////////////////////////////////////////////////////

#include "Plugin.h"
class LOREnhancedPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    LOREnhancedPlugin() :
        FPPPlugins::Plugin("LOREnhanced") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new LOREnhancedOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new LOREnhancedPlugin();
}
}

LOREnhancedOutput::LOREnhancedOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    data(nullptr) {}
LOREnhancedOutput::~LOREnhancedOutput() {
    if (data) {
        delete data;
    }
}

void LOREnhancedOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int LOREnhancedOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "LOREnhancedOutput::Close()\n");
    data->StopHeartbeat();
    data->closeSerialPort();
    return ChannelOutput::Close();
}

void LOREnhancedOutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    if (data) {
        data->dumpSerialConfig();
        LogDebug(VB_CHANNELOUT, "    port speed   : %d\n", data->speed);
        for (auto& unit : data->units) {
            LogDebug(VB_CHANNELOUT, "    Unit Id     : %d\n", unit.unitId);
            LogDebug(VB_CHANNELOUT, "      Pixels    : %d\n", unit.numOfPixels);
            LogDebug(VB_CHANNELOUT, "      Lor Start : %d\n", unit.lorStartPixel);
            LogDebug(VB_CHANNELOUT, "      FPP Start : %d\n", unit.fppStartAddr);
        }
    }
}

int LOREnhancedOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "LOREnhancedOutput::Init()\n");
    data = new LOREnhancedOutputData();

    if (config.isMember("speed")) {
        data->speed = config["speed"].asInt();
        // Older versions of the UI offered a mistyped 115400.
        if (data->speed == 115400) {
            data->speed = 115200;
        }
    }

    if (config.isMember("units")) {
        auto root = config["units"];
        for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
            LOREnhancedOutputUnit newUnit;
            if ((*it).isMember("unitId")) {
                newUnit.unitId = (*it)["unitId"].asInt();
            }
            if ((*it).isMember("numOfPixels")) {
                newUnit.numOfPixels = (*it)["numOfPixels"].asInt();
            }
            if ((*it).isMember("lorStartPixel")) {
                newUnit.lorStartPixel = (*it)["lorStartPixel"].asInt();
            }
            // 1-based.  A blank field arrives as JSON null and so as 0, which
            // sent the same packet as 1 (no leading jump) but made the
            // 510-channel check below count one pixel short: 171 pixels then
            // passed it and overran the 518 byte packet buffer.
            if (newUnit.lorStartPixel < 1) {
                newUnit.lorStartPixel = 1;
            }
            if ((*it).isMember("fppStartAddr")) {
                newUnit.fppStartAddr = (*it)["fppStartAddr"].asInt();
            }
            // SendUnitData indexes channelData[fppStartAddr - 1 + ...], so the
            // channel is 1-based and there is no valid zero.  The key is
            // optional and a blank UI field arrives as JSON null, both of which
            // asInt() turns into 0, so the unit has to be dropped here rather
            // than reaching the output path.
            if (newUnit.fppStartAddr < 1) {
                LogErr(VB_CHANNELOUT, "Invalid FPP Start Channel %d for LOR unit %d, ignoring the unit\n",
                       newUnit.fppStartAddr, newUnit.unitId);
                WarningHolder::AddWarning(39, "LOR unit " + std::to_string(newUnit.unitId) + ": invalid FPP start channel " + std::to_string(newUnit.fppStartAddr) + " - unit disabled");
                continue;
            }
            newUnit.lastSent.assign(std::max(newUnit.numOfPixels, 0) * 3, LOR_UNKNOWN);
            data->units.push_back(newUnit);
        }
    }
    if (!data->setupSerialPort(config, data->speed, "8N1")) {
        return 0;
    }
    data->StartHeartbeat();

    return ChannelOutput::Init(config);
}

bool LOREnhancedOutput::UnitIsValid(const LOREnhancedOutputUnit& unit) {
    if ((unit.lorStartPixel - 1) * 3 + unit.numOfPixels * 3 > LORE_MAX_CHANNELS_PER_UNIT) {
        LogErr(VB_CHANNELOUT, "More than 510 Channels, ie 170 pixels, per unit. Unit %d, Total channels: %d\n",
               unit.unitId, (unit.lorStartPixel - 1) * 3 + unit.numOfPixels * 3);
        WarningHolder::AddWarningTimeout(120, 39, "LOR unit " + std::to_string(unit.unitId) + ": more than 510 channels (170 pixels) per unit");
        return false;
    }
    // The LOR start pixel places the unit's pixels on the controller; it
    // does not move them in FPP's channel data, so it is not counted here.
    if (unit.fppStartAddr - 1 + unit.numOfPixels * 3 > (int)m_channelCount) {
        LogErr(VB_CHANNELOUT, "Invalid FPP Channel Range. Unit %d\n",
               unit.unitId);
        WarningHolder::AddWarningTimeout(120, 39, "LOR unit " + std::to_string(unit.unitId) + ": channel range exceeds the configured channel count");
        return false;
    }
    if (unit.unitId < 1 || unit.unitId > 240) {
        LogErr(VB_CHANNELOUT, "Invalid Unit ID. Unit %d\n",
               unit.unitId);
        WarningHolder::AddWarningTimeout(120, 39, "LOR: invalid unit ID " + std::to_string(unit.unitId) + " (must be 1-240)");
        return false;
    }
    return unit.numOfPixels > 0;
}

static inline uint16_t LORE_Value(unsigned char v) {
    return v ? v : 1;
}

// Writes an op's command byte(s) and returns how many it took.
static inline int LORE_PutCount(unsigned char* p, int op, int count) {
    if (count <= 30) {
        p[0] = op | count;
        return 1;
    }
    p[0] = op | 0x1F;
    p[1] = count;
    return 2;
}
static inline int LORE_CountBytes(int count) {
    return count <= 30 ? 1 : 2;
}
static int LORE_JumpBytes(int channels) {
    int bytes = 0;
    for (; channels > 0; channels -= 255) {
        bytes += LORE_CountBytes(std::min(channels, 255));
    }
    return bytes;
}

/*
 * Appends a packet bringing one unit up to date with cur, as far as room
 * bytes allow.  Unchanged pixels are jumped over, a run of two or more
 * identical pixels is one repeat (4 bytes against 6 or more as literals),
 * and the remaining changed pixels go as literals.  Works in whole pixels,
 * which is what was checked on hardware.  Pixels it covers are recorded as
 * sent.  Returns the bytes appended, 0 if there was nothing to send or not
 * even one op fit; complete says whether every changed pixel went.
 */
int LOREnhancedOutput::EncodeUnit(int unitIdx, const unsigned char* cur, unsigned char* out, int room, bool& complete) {
    LOREnhancedOutputUnit& unit = data->units[unitIdx];
    const int n = unit.numOfPixels;
    const int base = unit.lorStartPixel - 1;
    auto changed = [&](int p) {
        const uint16_t* last = &unit.lastSent[p * 3];
        return last[0] != LORE_Value(cur[p * 3]) || last[1] != LORE_Value(cur[p * 3 + 1]) || last[2] != LORE_Value(cur[p * 3 + 2]);
    };
    auto same = [&](int a, int b) {
        return LORE_Value(cur[a * 3]) == LORE_Value(cur[b * 3]) && LORE_Value(cur[a * 3 + 1]) == LORE_Value(cur[b * 3 + 1]) && LORE_Value(cur[a * 3 + 2]) == LORE_Value(cur[b * 3 + 2]);
    };

    complete = true;
    int len = 3;
    int streamAt = 0; // pixels from LOR pixel 1 the ops have reached
    int firstPixel = -1;
    int p = 0;
    while (p < n) {
        if (!changed(p)) {
            p++;
            continue;
        }
        int run = 1;
        while (p + run < n && run < 255 && same(p, p + run)) {
            run++;
        }
        bool repeat = run >= 2;
        int count = run;
        if (!repeat) {
            // Literal over the following changed pixels, stopping at an
            // unchanged one (a jump is cheaper) or at the start of a run.
            count = 1;
            while (p + count < n && count < 85 && changed(p + count) && !(p + count + 1 < n && same(p + count, p + count + 1))) {
                count++;
            }
        }
        int jump = (base + p - streamAt) * 3;
        int avail = room - len - 1 - LORE_JumpBytes(jump); // 1 for the closing 00
        int opBytes = repeat ? LORE_CountBytes(count) + 3 : LORE_CountBytes(count * 3) + count * 3;
        if (opBytes > avail && !repeat) {
            while (count > 0 && LORE_CountBytes(count * 3) + count * 3 > avail) {
                count--;
            }
            opBytes = LORE_CountBytes(count * 3) + count * 3;
        }
        if (count == 0 || opBytes > avail) {
            complete = false;
            break;
        }
        for (; jump > 0; jump -= 255) {
            len += LORE_PutCount(out + len, 0x20, std::min(jump, 255));
        }
        if (repeat) {
            len += LORE_PutCount(out + len, 0x40, count);
            for (int c = 0; c < 3; c++) {
                out[len++] = LORE_Value(cur[p * 3 + c]);
            }
        } else {
            len += LORE_PutCount(out + len, 0x00, count * 3);
            for (int c = 0; c < count * 3; c++) {
                out[len++] = LORE_Value(cur[p * 3 + c]);
            }
        }
        for (int c = p * 3; c < (p + count) * 3; c++) {
            unit.lastSent[c] = LORE_Value(cur[c]);
        }
        if (firstPixel < 0) {
            firstPixel = p;
        }
        p += count;
        streamAt = base + p;
    }
    if (firstPixel < 0) {
        return 0;
    }
    out[0] = 0x00;
    out[1] = unit.unitId;
    out[2] = LOR_ENHANCED_CMD;
    out[len++] = 0x00;
    data->sent.push_back({ unitIdx, firstPixel, p, 0 });
    return len;
}

int LOREnhancedOutput::SendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELDATA, "LOREnhancedOutput::SendData()\n");

    const int count = data->units.size();
    if (!count) {
        return m_channelCount;
    }

    // Changes that do not fit the frame's budget stay unsent and go out on
    // a later frame; a unit can be brought up to date over several frames
    // because a packet leaves everything it does not reach unchanged.
    int budget = data->FrameBudget();
    int len = 0;
    bool allSent = true;
    data->sent.clear();
    for (int k = 0; k < count; k++) {
        int u = (data->nextUnit + k) % count;
        LOREnhancedOutputUnit& unit = data->units[u];
        if (!UnitIsValid(unit)) {
            continue;
        }
        bool complete = true;
        int added = EncodeUnit(u, channelData + unit.fppStartAddr - 1, data->batch + len, budget - len, complete);
        if (added) {
            len += added;
            data->sent.back().batchEnd = len;
        }
        if (!complete) {
            data->nextUnit = u;
            allSent = false;
            break;
        }
    }

    if (len) {
        int written = data->WriteBatch(data->batch, len);
        if (written < len) {
            // A cut-off packet may have been applied in part or not at all,
            // so everything it and the packets after it reached is resent.
            for (auto& s : data->sent) {
                if (s.batchEnd > written) {
                    LOREnhancedOutputUnit& unit = data->units[s.unit];
                    std::fill(unit.lastSent.begin() + s.firstPixel * 3, unit.lastSent.begin() + s.endPixel * 3, LOR_UNKNOWN);
                }
            }
            allSent = false;
        }
    }

    // With everything up to date, mark a few pixels for resending.  A unit
    // that missed a packet (line noise, power cycled) would otherwise hold
    // the wrong values until they next changed, which for a static scene is
    // never.  One pass over all the pixels per LOR_REFRESH_PERIOD at most.
    if (allSent) {
        int total = 0;
        for (auto& unit : data->units) {
            total += std::max(unit.numOfPixels, 0);
        }
        int refresh = std::max(1, (int)((total * data->frameTime + LOR_REFRESH_PERIOD - 1) / LOR_REFRESH_PERIOD));
        for (int checked = 0; refresh > 0 && checked < count; ) {
            LOREnhancedOutputUnit& unit = data->units[data->refreshUnit % count];
            if (data->refreshPixel >= unit.numOfPixels) {
                data->refreshUnit = (data->refreshUnit + 1) % count;
                data->refreshPixel = 0;
                checked++;
                continue;
            }
            int take = std::min(refresh, unit.numOfPixels - data->refreshPixel);
            std::fill(unit.lastSent.begin() + data->refreshPixel * 3, unit.lastSent.begin() + (data->refreshPixel + take) * 3, LOR_UNKNOWN);
            data->refreshPixel += take;
            refresh -= take;
            checked = 0;
        }
    }
    return m_channelCount;
}
