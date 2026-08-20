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
#include <tuple>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>

// FPP includes
#include "../../Sequence.h"
#include "../../Warnings.h"
#include "../../common.h"
#include "../../log.h"
#include "../../settings.h"

#include "BBShiftPanel.h"
#include "../../channeloutput/GammaLUT.h"
#include "../CapeUtils/CapeUtils.h"
#include "../../pru/SMEMRing.hp"
#include "util/BBBUtils.h"

#include "MapPixelsByDepth16.h"

#include "channeloutput/PanelInterleaveHandler.h"
#include "overlays/PixelOverlay.h"

#include "Plugin.h"
class BBShiftPanelPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    BBShiftPanelPlugin() :
        FPPPlugins::Plugin("BBShiftPanel") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new BBShiftPanelOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new BBShiftPanelPlugin();
}
}

constexpr int ADDRESSING_MODE_STANDARD = 0;
constexpr int ADDRESSING_MODE_AB_SHIFT = 1;  // row shift register, A=clock, B=data (active low)
constexpr int ADDRESSING_MODE_DIRECT = 2;
constexpr int ADDRESSING_MODE_ABC_SHIFT = 3; // row shift register, A=clock, C=data (active high)
constexpr int ADDRESSING_MODE_ABC_DE = 4;    // SM5266P: A/B/C drive a shifter, D/E select the bank
constexpr int ADDRESSING_MODE_FM6353C = 50;
constexpr int ADDRESSING_MODE_FM6363C = 51;
constexpr int ADDRESSING_MODE_FM6373 = 52;
constexpr int ADDRESSING_MODE_DP3364 = 53;
constexpr int ADDRESSING_MODE_ICND1065L = 54;
constexpr int ADDRESSING_MODE_SM16380SH = 55;
constexpr int ADDRESSING_MODE_ICND2153 = 56;

constexpr int PANEL_TYPE_FM6126A = 1;
constexpr int PANEL_TYPE_FM6127 = 2;
// The PWM chips are presented in the UI as panel types (they are driver
// chips like the FM6126A), but internally they are PWM addressing modes
constexpr int PANEL_TYPE_FM6363C = 3;
constexpr int PANEL_TYPE_FM6353 = 4;
constexpr int PANEL_TYPE_FM6373 = 5; // the common DP32019B receiver boards
constexpr int PANEL_TYPE_DP3364 = 6;
constexpr int PANEL_TYPE_ICND1065L = 7;
constexpr int PANEL_TYPE_SM16380SH = 8;
// Split out of PANEL_TYPE_FM6353, which used to claim ICN2153 compatibility.
// The two take the same register grammar but not the same values, so an
// existing config on type 4 keeps the FM6353 payload it has always had.
constexpr int PANEL_TYPE_ICND2153 = 9;

// must match PWM_CHIP_CONFIG_OFFSET in BBShiftPanel_pwm.asm
#define PWM_CHIP_CONFIG_OFFSET 0x1DF8
// the register slots live at the front of the same PRU data RAM, so growing
// them must not reach the chip config word
static_assert(offsetof(BBShiftPanelData, registers) + sizeof(((BBShiftPanelData*)nullptr)->registers) <= PWM_CHIP_CONFIG_OFFSET);
// REG1_OFF in BBShiftPanel_pwm.asm is this offset, hardcoded as 16
static_assert(offsetof(BBShiftPanelData, registers) == 16);

// FM6373 config register sequence, one {address, value} word per register,
// different per color line.  Captured from a working DP32019B 128x64 panel
// by the kingdo9/rpi-rgb-led-matrix_pwm_experiment project
// (lib/spwm-panel-registers.cc, register config 0); DMD_STM32 carries a
// shorter single-color variant of the same address space.  The 0x02
// register holds the scan row count and is patched at runtime.
static const uint16_t FM6373_SEQ_R[] = {
    0x0000, 0x0100, 0x021f, 0x033f, 0x0402, 0x0508, 0x0602, 0x0720,
    0x0820, 0x0900, 0x0a00, 0x0b00, 0x0c01, 0x0d01, 0x0e04, 0x0f01,
    0x10c2, 0x1121, 0x1201, 0x1300, 0x1400, 0x1500, 0x1600, 0x17f0,
    0x181f, 0x1900, 0x1a1f, 0x1b10, 0x1c2a, 0x1d0a, 0x1e42, 0x1f04,
    0x2008, 0x2101, 0x221c, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400,
    0xf000, 0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0x2300
};
static const uint16_t FM6373_SEQ_G[] = {
    0x0000, 0x0100, 0x021f, 0x033f, 0x0402, 0x0508, 0x0602, 0x0720,
    0x0820, 0x0900, 0x0a00, 0x0b00, 0x0c08, 0x0d01, 0x0e04, 0x0f01,
    0x10c2, 0x1121, 0x1201, 0x1300, 0x1400, 0x1500, 0x1600, 0x17f0,
    0x181f, 0x1950, 0x1a1f, 0x1b10, 0x1c2a, 0x1d0a, 0x1e46, 0x1f20,
    0x2008, 0x2101, 0x221c, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400,
    0xf000, 0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0x2300
};
static const uint16_t FM6373_SEQ_B[] = {
    0x0000, 0x0100, 0x021f, 0x033f, 0x0402, 0x0508, 0x0602, 0x0720,
    0x0820, 0x0900, 0x0a00, 0x0b00, 0x0c08, 0x0d01, 0x0e04, 0x0f01,
    0x10c2, 0x1121, 0x1201, 0x1300, 0x1400, 0x1500, 0x1600, 0x17f0,
    0x182f, 0x1900, 0x1a1f, 0x1b10, 0x1c2a, 0x1d0a, 0x1e48, 0x1f20,
    0x2010, 0x2101, 0x221c, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400,
    0xf000, 0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0x2300
};
constexpr int FM6373_SEQ_LEN = 47;
static_assert(sizeof(FM6373_SEQ_R) == FM6373_SEQ_LEN * sizeof(uint16_t));
static_assert(sizeof(FM6373_SEQ_G) == FM6373_SEQ_LEN * sizeof(uint16_t));
static_assert(sizeof(FM6373_SEQ_B) == FM6373_SEQ_LEN * sizeof(uint16_t));

// FM6373 registers for a full height 1/64 scan panel, from the same project
// (fm6373.profiles, fm6373_regtype2, a 128x64 capture).  Nine registers differ
// from the 1/32 table above beyond the scan count, and the whole 0x70/0xf0
// tail block moves to a repeated 0x54 - which is why patching only the scan
// count does not turn a 1/32 table into a working 1/64 one.
static constexpr uint16_t FM6373_64S_SEQ_R[] = {
    0x0000, 0x0100, 0x023f, 0x033f, 0x0402, 0x0508, 0x0602, 0x0710,
    0x0810, 0x0900, 0x0a00, 0x0b00, 0x0c01, 0x0d03, 0x0e02, 0x0f11,
    0x10c2, 0x1121, 0x1201, 0x1300, 0x1400, 0x1500, 0x1600, 0x17f0,
    0x181f, 0x1900, 0x1a1f, 0x1b10, 0x1cbe, 0x1d0e, 0x1e42, 0x1f24,
    0x2008, 0x2101, 0x221c, 0x5400, 0x5400, 0x5400, 0x5400, 0x5400,
    0x5403, 0x5403, 0x5403, 0x5403, 0x5403, 0x5403, 0x2300
};
static constexpr uint16_t FM6373_64S_SEQ_G[] = {
    0x0000, 0x0100, 0x023f, 0x033f, 0x0402, 0x0508, 0x0602, 0x0710,
    0x0810, 0x0900, 0x0a00, 0x0b00, 0x0c08, 0x0d03, 0x0e04, 0x0f11,
    0x10c2, 0x1121, 0x1201, 0x1300, 0x1400, 0x1500, 0x1600, 0x17f0,
    0x181f, 0x1950, 0x1a1f, 0x1b10, 0x1cbe, 0x1d0e, 0x1e46, 0x1f20,
    0x2008, 0x2101, 0x221c, 0x5400, 0x5400, 0x5400, 0x5400, 0x5400,
    0x5403, 0x5403, 0x5403, 0x5403, 0x5403, 0x5403, 0x2300
};
static constexpr uint16_t FM6373_64S_SEQ_B[] = {
    0x0000, 0x0100, 0x023f, 0x033f, 0x0402, 0x0508, 0x0602, 0x0710,
    0x0810, 0x0900, 0x0a00, 0x0b00, 0x0c08, 0x0d01, 0x0e04, 0x0f11,
    0x10c2, 0x1121, 0x1201, 0x1300, 0x1400, 0x1500, 0x1600, 0x17f0,
    0x182f, 0x1900, 0x1a1f, 0x1b10, 0x1cbe, 0x1d0e, 0x1e48, 0x1f20,
    0x2010, 0x2101, 0x221c, 0x5400, 0x5400, 0x5400, 0x5400, 0x5400,
    0x5403, 0x5403, 0x5403, 0x5403, 0x5403, 0x5403, 0x2300
};

// ICND1065L config registers.  From kingdo9/rpi-rgb-led-matrix_pwm_experiment
// (lib/spwm/registertest/data/icnd1065l.profiles, icnd1065l_regtype1 - the
// project's built-in default, captured from a 1/43 scan panel).  The chip
// takes the same upload grammar as the FM6373, so only the payload differs.
static constexpr uint16_t ICND1065L_SEQ_R[] = {
    0x0000, 0x026a, 0x0322, 0x0412, 0x0500, 0x0601, 0x0712, 0x0c10,
    0x0d02, 0x0e84, 0x0f01, 0x1040, 0x1127, 0x1800, 0x1926, 0x1c60,
    0x1d02, 0x1e71, 0x2040, 0x2101, 0x2380, 0x74a0
};
static constexpr uint16_t ICND1065L_SEQ_G[] = {
    0x0000, 0x026a, 0x0322, 0x0412, 0x0500, 0x0601, 0x0712, 0x0c10,
    0x0d04, 0x0e84, 0x0f01, 0x1040, 0x1127, 0x1800, 0x1908, 0x1c60,
    0x1d02, 0x1e92, 0x2060, 0x2101, 0x2305, 0x74a0
};
static constexpr uint16_t ICND1065L_SEQ_B[] = {
    0x0000, 0x026a, 0x0322, 0x0412, 0x0500, 0x0601, 0x0712, 0x0c10,
    0x0d03, 0x0e84, 0x0f11, 0x1040, 0x1127, 0x1800, 0x190a, 0x1c60,
    0x1d02, 0x1eb5, 0x2060, 0x2101, 0x2300, 0x74a0
};
constexpr int ICND1065L_SEQ_LEN = 22;

// SM16380SH config registers, same source (spwm-panel-registers.cc built-in
// main block, a 1/32 scan capture).  This chip takes a sixth slot carrying
// 0xF003 ahead of the commit pair, and skips the FM6373's middle LAT burst.
static constexpr uint16_t SM16380SH_SEQ_R[] = {
    0x021f, 0x0300, 0x0400, 0x0500, 0x0600, 0x078c, 0x0800, 0x0900,
    0x0a02, 0x0b0c, 0x0c08, 0x0d00, 0x0e05, 0x0f00, 0x1000, 0x1100,
    0x1200, 0x1308, 0x1414, 0x1500, 0x1630, 0x1700, 0x1801, 0x1904,
    0x1a03, 0x1b14, 0x1c12, 0x1d00, 0x1e00, 0x1f0c, 0x2000, 0x2200
};
static constexpr uint16_t SM16380SH_SEQ_G[] = {
    0x021f, 0x0300, 0x0400, 0x0500, 0x0600, 0x078c, 0x0800, 0x0900,
    0x0a02, 0x0b0c, 0x0c18, 0x0d00, 0x0e05, 0x0f00, 0x1000, 0x1100,
    0x1200, 0x1308, 0x1422, 0x1500, 0x1630, 0x1700, 0x1801, 0x1903,
    0x1a01, 0x1b14, 0x1c8f, 0x1d00, 0x1e00, 0x1f0c, 0x2000, 0x2200
};
static constexpr uint16_t SM16380SH_SEQ_B[] = {
    0x021f, 0x0300, 0x0400, 0x0500, 0x0600, 0x078c, 0x0800, 0x0900,
    0x0a02, 0x0b0c, 0x0c30, 0x0d00, 0x0e05, 0x0f00, 0x1000, 0x1100,
    0x1200, 0x1308, 0x1432, 0x1500, 0x1630, 0x1700, 0x1801, 0x1903,
    0x1a01, 0x1b14, 0x1c8f, 0x1d00, 0x1e00, 0x1f0c, 0x2000, 0x2200
};
constexpr int SM16380SH_SEQ_LEN = 32;

// The FM6373, ICND1065L and SM16380SH uploads are one grammar: a few bare LAT
// bursts, then N 16-bit slots each latched over their last 5 clocks, with slot
// 3 rotating through the chip's config sequence one word per frame.  They
// differ only in the payload, whether the middle (11 clock) LAT burst is sent,
// how many slots follow, and which register carries the scan count.
// A capture taken at one scan rate.  Registers other than the scan count
// change with the scan rate on these chips, so a table is only really valid
// for the geometry it came from; where a second capture is known the closest
// one is used instead of stretching the default.
struct PWMChipSeqVariant {
    int scan;
    const uint16_t* r;
    const uint16_t* g;
    const uint16_t* b;
};

struct PWMChipSeq {
    const uint16_t* r;
    const uint16_t* g;
    const uint16_t* b;
    int len;
    int slots;          // 5, or 6 for a chip with an extra pre-commit word
    uint16_t extraWord; // that word; only read when slots == 6
    bool midLatch;      // send the 11 clock LAT burst
    uint8_t scanReg;    // register address holding the scan row count
    int defaultScan;    // scan rate the tables above were captured at
    const PWMChipSeqVariant* variants;
    int variantCount;
};

static const PWMChipSeq* pwmChipSeqFor(int addressingMode) {
    // Scan count: every profile in the kingdo9 catalog puts (rows - 1) in the
    // low 6 bits of this register and leaves the upper 2 bits as a chip
    // constant, so the patch preserves whatever the table above carries there
    // (ICND1065L sets bit 6; the other two leave it clear).
    static const PWMChipSeqVariant FM6373_VARIANTS[] = {
        { 64, FM6373_64S_SEQ_R, FM6373_64S_SEQ_G, FM6373_64S_SEQ_B },
    };
    static const PWMChipSeq FM6373_SEQ = {
        FM6373_SEQ_R, FM6373_SEQ_G, FM6373_SEQ_B, FM6373_SEQ_LEN, 5, 0, true, 0x02,
        32, FM6373_VARIANTS, 1
    };
    static const PWMChipSeq ICND1065L_SEQ = {
        ICND1065L_SEQ_R, ICND1065L_SEQ_G, ICND1065L_SEQ_B, ICND1065L_SEQ_LEN, 5, 0, true, 0x02,
        43, nullptr, 0
    };
    static const PWMChipSeq SM16380SH_SEQ = {
        SM16380SH_SEQ_R, SM16380SH_SEQ_G, SM16380SH_SEQ_B, SM16380SH_SEQ_LEN, 6, 0xF003, false, 0x02,
        32, nullptr, 0
    };
    switch (addressingMode) {
    case ADDRESSING_MODE_FM6373:
        return &FM6373_SEQ;
    case ADDRESSING_MODE_ICND1065L:
        return &ICND1065L_SEQ;
    case ADDRESSING_MODE_SM16380SH:
        return &SM16380SH_SEQ;
    }
    return nullptr;
}

// DP3364S config registers, one {address, value} word per register, different
// per color line.  Captured from a working 128x64 1/64 scan panel and posted
// to hzeller/rpi-rgb-led-matrix issue #1821; the chip's own datasheet
// documents the access protocol but not the register map.
//
// The datasheet says 15 valid register addresses and that 15 frames complete
// a full refresh, which is exactly 0x02-0x0F plus 0x15 - the addresses the
// capture carries values for (0x10-0x14 all read back zero and are taken as
// reserved).  Documented meanings, for the ones that are documented:
//   0x03[6:0] PWM display packet count - 1 (0x3f = 64, the async mode default)
//   0x04[6:0] row PWM display length - 1
//   0x06[2:0] internal GCLK multiplier, FGCLK = FDCLK * (n + 1)
//   0x08[7:0] linear output current multiplier - the brightness knob, patched
//             at runtime; the captured 0x7f is what the panel was built for,
//             so that is full brightness and FPP only scales down from it
//   0x0b[5]   1.5x current gain
//   0x0c[7:6] PWM display mode (01 = high gray data independent refresh, the
//             free running row scan this driver generates); [1] = drop open
//             circuit bad spots
//   0x0f[6:0] current reference
// 0x02 is undocumented but holds 0x3f on a 64 row panel and is patched at
// runtime with the scan count, the same as 0x02 on the FM6373.
static constexpr uint16_t DP3364_SEQ_R[] = {
    0x023f, 0x033f, 0x041a, 0x0504, 0x0639, 0x0700, 0x087f, 0x0968,
    0x0abe, 0x0b28, 0x0c58, 0x0d08, 0x0e08, 0x0f20, 0x1500
};
static constexpr uint16_t DP3364_SEQ_G[] = {
    0x023f, 0x033f, 0x041a, 0x0504, 0x0639, 0x070c, 0x087f, 0x096b,
    0x0abf, 0x0b2b, 0x0c58, 0x0d12, 0x0e0b, 0x0f20, 0x1504
};
static constexpr uint16_t DP3364_SEQ_B[] = {
    0x023f, 0x033f, 0x041a, 0x0500, 0x0639, 0x070c, 0x087f, 0x0961,
    0x0abe, 0x0b31, 0x0c58, 0x0d18, 0x0e01, 0x0f20, 0x1504
};
constexpr int DP3364_SEQ_LEN = 15;
// setupGCLKConfig reads the group count out of the 0x03 entry
static_assert((DP3364_SEQ_R[1] >> 8) == 0x03);
static_assert(sizeof(DP3364_SEQ_R) == DP3364_SEQ_LEN * sizeof(uint16_t));
static_assert(sizeof(DP3364_SEQ_G) == DP3364_SEQ_LEN * sizeof(uint16_t));
static_assert(sizeof(DP3364_SEQ_B) == DP3364_SEQ_LEN * sizeof(uint16_t));

// Approximate DCLK period in nanoseconds for the two PWM firmware variants,
// from the instruction counts in BBShiftPanel_pwm.asm at the AM62x PRU's 4ns
// cycle: OUTPUT_PIXEL is 24 cycles for 8 outputs and 42 for 16, plus a ~26
// cycle LOAD_DATA every 8 (resp. 4) pixels.
constexpr int DCLK_NS_8 = 109;
constexpr int DCLK_NS_16 = 194;

// Minimum DCLK periods the DP3364S needs per displayed row, from the "time of
// one line" formula the datasheet repeats for each PWM display mode:
//   (2*(reg0x05[7:4]+1) + 2*(reg0x05[3:0]+1) + 4*(reg0x04[6:0]+1))
//       / (reg0x06[2:0]+1)
// The three color lines are separate chips and need not agree, so the scan has
// to hold for the slowest of them.
static int dp3364MinLineDCLKs(const uint16_t* seq, int len) {
    int r04 = 0, r05 = 0, r06 = 0;
    for (int i = 0; i < len; i++) {
        switch (seq[i] >> 8) {
        case 0x04:
            r04 = seq[i] & 0x7F;
            break;
        case 0x05:
            r05 = seq[i] & 0xFF;
            break;
        case 0x06:
            r06 = seq[i] & 0x07;
            break;
        }
    }
    return (2 * (((r05 >> 4) & 0x0F) + 1) + 2 * ((r05 & 0x0F) + 1) + 4 * (r04 + 1)) / (r06 + 1);
}

constexpr int PWM_COMMAND_SYNC = 0x0001;
constexpr int PWM_COMMAND_REGISTERS = 0x0002;
constexpr int PWM_COMMAND_STARTGCLK = 0x0004;
constexpr int PWM_COMMAND_DATA = 0x0008;
constexpr int PWM_COMMAND_HALT = 0xFFFF;

// Note: the ordering of the bit planes within a frame (formerly a set of
// hand-tuned BIT_ORDERS tables) is now computed by buildStrideSchedule(),
// which also splits the long MSB on-times into multiple pulses spread
// across the frame to raise the perceived refresh rate.

static const std::vector<std::string> PRU_PINS = { "P1-20", "P1-29", "P1-31", "P1-33", "P1-36",
                                                   "P2-02", "P2-04", "P2-06", "P2-08",
                                                   "P2-18", "P2-20", "P2-22", "P2-24",
                                                   "P2-28", "P2-30", "P2-32", "P2-33",
                                                   "P2-34", "P2-35" };

static const std::vector<std::string> PRU0_PWM_PINS = {
    "P1-31", "P2-28", "P2-30", "P2-32", "P2-34", "P1-36"
};

constexpr int MAX_OUTPUTS = 16;

// how much the pump thread copies into the shared memory ring at a time; a
// multiple of 48 and 8 (the ring write splits blocks across the wrap point,
// so it does not need to divide the ring size evenly)
constexpr uint32_t PUMP_BLOCK_SIZE = 3072;

BBShiftPanelManager BBShiftPanelManager::INSTANCE;

BBShiftPanelOutput::BBShiftPanelOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::BBShiftPanelOutput(%u, %u)\n",
             startChannel, channelCount);
}

void BBShiftPanelOutput::StartingOutput() {
    // StoppingOutput is also called when the channel output thread simply
    // goes idle, so the stopping flag has to clear when output resumes
    m_stopping = false;
    BBShiftPanelManager::INSTANCE.startingOutput();
}

void BBShiftPanelManager::startingOutput() {
    if (m_activeMembers++) {
        // the cape is already refreshing for another matrix
        return;
    }
    // Restart the refresh before the init registers go out below: the PRU
    // has to be running to see them at all, and it only reads commands at a
    // frame boundary, so if it is still working through a frame it needs the
    // pump running to get there.
    UnparkPRUs();
    if (!isPWMPanel() && m_panelType && pruData) {
        // FM6126A style panels need their configuration registers clocked
        // out before they will display anything; the registers can be lost
        // if the panels lose power, so resend them whenever output starts
        sendPanelInitPackets();
    }
}

// must match PANEL_INIT_OFFSET / ADDR_CONFIG_OFFSET in BBShiftPanel.asm
#define PANEL_INIT_OFFSET 0x1E00
#define ADDR_CONFIG_OFFSET 0x1DF8

void BBShiftPanelManager::sendPanelInitPackets() {
    // panel configuration register writes, from the rpi-rgb-led-matrix
    // project (lib/framebuffer.cc InitFM6126/InitFM6127): a register write
    // is the pattern repeated across the whole chain width on every data
    // line with LATCH held through the last (leCount-1) column clocks.
    // Pattern bit i is the data value for column i mod 16.
    auto encode = [](const char* p) {
        uint16_t v = 0;
        for (int i = 0; i < 16; i++) {
            if (p[i] == '1') {
                v |= 1 << i;
            }
        }
        return v;
    };
    struct InitReg {
        uint16_t leCount;
        uint16_t pattern;
    };
    std::vector<InitReg> regs;
    if (m_panelType == PANEL_TYPE_FM6126A) {
        // FM6126A: config register 1 (full brightness), register 2 (panel on)
        regs.push_back({ 12, encode("0111111111111111") });
        regs.push_back({ 13, encode("0000000001000000") });
    } else if (m_panelType == PANEL_TYPE_FM6127) {
        // FM6127: registers 1-3
        regs.push_back({ 12, encode("1111111111001110") });
        regs.push_back({ 13, encode("1110000001100010") });
        regs.push_back({ 11, encode("0101111100000000") });
    } else {
        return;
    }
    // the PRU data RAM cannot take a plain memcpy (uncacheable segment,
    // SIGBUS on aarch64) - memcpyToPRU handles it
    uint32_t buf[16] = { 0 };
    buf[0] = (uint32_t)regs.size() | (((uint32_t)rowLen) << 16);
    for (size_t i = 0; i < regs.size(); i++) {
        buf[i + 1] = (uint32_t)regs[i].leCount | (((uint32_t)regs[i].pattern) << 16);
    }
    pru->memcpyToPRU(pru->data_ram + PANEL_INIT_OFFSET, (uint8_t*)buf, sizeof(buf));
    __sync_synchronize();
    pruData->command = 0xFFF0;
    int cnt = 0;
    while (pruData->command == 0xFFF0 && cnt < 250) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cnt++;
    }
    if (pruData->command == 0xFFF0) {
        LogWarn(VB_CHANNELOUT, "BBShiftPanel: panel init registers were not sent (PRU did not respond)\n");
    } else {
        LogDebug(VB_CHANNELOUT, "BBShiftPanel: sent %zu panel init registers for panel type %d\n", regs.size(), m_panelType);
    }
}

void BBShiftPanelOutput::StoppingOutput() {
    // Called when the channel output thread stops and before the output is
    // deleted on a config reload; the thread may still be inside
    // PrepData/SendData, so refuse new work and wait for any in-flight call
    // to drain so a teardown cannot race them.
    m_stopping = true;
    while (m_inFlight > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    BBShiftPanelManager::INSTANCE.stoppingOutput();
}

void BBShiftPanelManager::stoppingOutput() {
    if (m_activeMembers > 0 && --m_activeMembers) {
        // another matrix on this cape is still outputting
        return;
    }
    // a partially arrived or prepped frame must not carry across the park, or
    // the first frame after the restart would publish stale content
    m_preppedThisFrame = 0;
    m_framePrepped = false;
    ParkPRUs();
}

// The channel output thread only stops once nothing wants to output at all
// (forceOutput() in channeloutputthread.cpp covers sequences, overlays,
// effects, testing and bridging), and a shift panel only holds an image for
// as long as the pump keeps re-shifting it - an idle panel is simply dark.
// Streaming that blank frame costs most of a core for as long as fppd runs,
// so park instead:
//   - the pump stops on a frame boundary and clears the command word
//   - the PRU reads that at the end of the frame it is in and parks in its
//     command wait loop, which holds the display off; the OE PRU parks the
//     same way once the brightness handshake stops
//   - both cores are then halted, which also freezes their cycle counters
// That last part is not just about saving (already idle) PRU cycles: the
// counter saturates after 2^32 cycles (~17s) and the hardware clears CTR_EN
// when it does, which RESET_PRU_CLOCK cannot undo, and the OE PRU's
// GET_PRU_CLOCK spin loops would then never exit.  A halted core cannot
// reach that state.
//
// PWM panels refresh from their own registers and their pump already idles
// between commands, so none of this applies to them.
void BBShiftPanelManager::ParkPRUs() {
    if (isPWMPanel() || !pru || !pruData || m_prusPaused) {
        return;
    }
    m_pumpPaused = true;
    // The pump only acts on this at a frame boundary, so it has written
    // whole frames only.
    int cnt = 0;
    while (m_pumpRunning && !m_pumpParked && cnt < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cnt++;
    }
    // Then the frame still in flight has to drain, so the PRU ends up on the
    // same frame boundary the pump stopped on and the two stay aligned in
    // the byte stream.  The ring is far smaller than a frame, so this is at
    // most that frame's tail.
    for (cnt = 0; !m_ring.drained() && cnt < 500; cnt++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!m_pumpParked || !m_ring.drained()) {
        // never leave the cores halted somewhere unknown - a frozen mid-scan
        // output would hold one row lit
        LogWarn(VB_CHANNELOUT, "BBShiftPanel: could not park (pumpParked=%d drained=%d), leaving the refresh running\n",
                (int)m_pumpParked, (int)m_ring.drained());
        m_pumpPaused = false;
        return;
    }
    // The OE PRU only leaves its wait loop when this PRU hands it a
    // brightness, so with this PRU parked one more display period is enough
    // for both to be sitting in their command wait loops with the display
    // off.  Halting anywhere else would freeze the pins mid-scan.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pru->pause();
    if (pwmPru) {
        pwmPru->pause();
    }
    m_prusPaused = true;
}

void BBShiftPanelManager::UnparkPRUs() {
    if (m_prusPaused) {
        // the OE PRU first, so it is already waiting by the time the panel
        // PRU starts handing it brightnesses
        if (pwmPru) {
            pwmPru->resume();
        }
        if (pru) {
            pru->resume();
        }
        m_prusPaused = false;
    }
    m_pumpPaused = false;
}

BBShiftPanelOutput::~BBShiftPanelOutput() {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::~BBShiftPanelOutput()\n");

    m_stopping = true;
    while (m_inFlight > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // covers an Init that failed after the manager accepted this matrix, where
    // the output is deleted without Close() ever being called
    releaseToManager();
    BBShiftPanelManager::INSTANCE.removeMember(this);

    if (m_matrix)
        delete m_matrix;
    if (m_panelMatrix)
        delete m_panelMatrix;
}

void BBShiftPanelManager::removeMember(BBShiftPanelOutput* m) {
    auto it = std::find(m_members.begin(), m_members.end(), m);
    if (it == m_members.end()) {
        // never registered (its Init failed)
        return;
    }
    m_members.erase(it);
    m_preppedThisFrame = 0;
    m_framePrepped = false;
    // The remaining members keep the geometry they were mapped against.  It
    // stays valid - a rowLen longer than the remaining chains need just shifts
    // a few extra pixels off the end of those chains - and rebuilding here
    // would restart the PRU in the middle of a shutdown.
    if (m_members.empty()) {
        teardown();
    }
}

// Stops the hardware but keeps the parsed configuration, so a rebuild can
// bring it back up with new geometry.
void BBShiftPanelManager::teardownHardware() {
    stopBackgroundThreads();
    StopPRU();
    if (m_heapBuffers) {
        for (auto& b : outputBuffers) {
            if (b) {
                free(b);
                b = nullptr;
            }
        }
        m_heapBuffers = false;
    }
    m_frontBuffer = nullptr;
    currOutputBuffer = 0;
}

void BBShiftPanelManager::teardown() {
    teardownHardware();
    if (currentChannelData) {
        delete[] currentChannelData;
        currentChannelData = nullptr;
    }
    if (!m_refreshWarning.empty()) {
        WarningHolder::RemoveWarning(m_refreshWarning);
        m_refreshWarning.clear();
    }
    m_activeMembers = 0;
    m_openMembers = 0;
}

bool BBShiftPanelManager::isPWMPanel() const {
    return m_addressingMode >= ADDRESSING_MODE_FM6353C;
}

int BBShiftPanelOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::Init(JSON)\n");

    Json::Value root;
    std::string subType = config["configName"].asString();
    int outputs = CapeUtils::INSTANCE.getLicensedOutputs();
    if (!CapeUtils::INSTANCE.getPanelConfig(subType, root)) {
        LogErr(VB_CHANNELOUT, "Could not read panel pin configuration for %s\n", subType.c_str());
        return 0;
    }

    // Everything below here is this matrix's own: its pixel mapping, its
    // channel range, its color handling.  The hardware the frame goes out
    // on is shared with any other matrix on the same cape and is set up by
    // the manager in addMember() below.
    m_invertedData = config["invertedData"].asInt();
    m_colorOrder = ColorOrderFromString(config["colorOrder"].asString());
    if (config.isMember("brightness")) {
        m_brightness = config["brightness"].asInt();
    }
    if (m_brightness < 1 || m_brightness > 10) {
        m_brightness = 10;
    }
    m_gamma = 2.2;
    if (config.isMember("gamma")) {
        m_gamma = atof(config["gamma"].asString().c_str());
    }

    int panelWidth = config["panelWidth"].asInt();
    int panelHeight = config["panelHeight"].asInt();
    if (!panelWidth) {
        panelWidth = 32;
    }
    if (!panelHeight) {
        panelHeight = 16;
    }

    m_panelMatrix = new PanelMatrix(panelWidth, panelHeight, m_invertedData);
    if (!m_panelMatrix) {
        LogErr(VB_CHANNELOUT, "BBShiftPanelOutput: Unable to create PanelMatrix\n");
        return 0;
    }
    for (int i = 0; i < config["panels"].size(); i++) {
        Json::Value p = config["panels"][i];
        if (p["outputNumber"].asInt() <= outputs) {
            char orientation = 'N';
            std::string o = p["orientation"].asString();

            if (!o.empty()) {
                orientation = o[0];
            }

            if (p["colorOrder"].asString() == "") {
                p["colorOrder"] = ColorOrderToString(m_colorOrder);
            }

            m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
                                    p["panelNumber"].asInt(),
                                    orientation,
                                    p["xOffset"].asInt(), p["yOffset"].asInt(),
                                    ColorOrderFromString(p["colorOrder"].asString()));
            if (p["panelNumber"].asInt() > m_longestChain) {
                m_longestChain = p["panelNumber"].asInt();
            }
        }
    }
    m_longestChain++;

    // get the dimensions of the matrix
    m_panels = m_panelMatrix->PanelCount();
    m_width = m_panelMatrix->Width();
    m_height = m_panelMatrix->Height();

    m_channelCount = m_width * m_height * 3;

    m_matrix = new Matrix(m_startChannel, m_width, m_height);
    if (config.isMember("subMatrices")) {
        for (int i = 0; i < config["subMatrices"].size(); i++) {
            Json::Value sm = config["subMatrices"][i];

            m_matrix->AddSubMatrix(
                sm["enabled"].asInt(),
                sm["startChannel"].asInt() - 1,
                sm["width"].asInt(),
                sm["height"].asInt(),
                sm["xOffset"].asInt(),
                sm["yOffset"].asInt());
        }
    }

    // Claims the outputs this matrix drives on the shared cape and brings the
    // PRU up (or rebuilds it around the new geometry if another matrix is
    // already running).  This is what builds the gamma table and the scatter
    // map, since both depend on the shared frame layout.
    if (!BBShiftPanelManager::INSTANCE.addMember(this, config, root)) {
        return 0;
    }
    m_registered = true;

    if (PixelOverlayManager::INSTANCE.isAutoCreatePixelOverlayModels()) {
        std::string dd = "LED Panels";
        if (config.isMember("LEDPanelMatrixName") && !config["LEDPanelMatrixName"].asString().empty()) {
            dd = config["LEDPanelMatrixName"].asString();
        }
        if (config.isMember("description")) {
            dd = config["description"].asString();
        }
        std::string desc = dd;
        int count = 0;
        while (PixelOverlayManager::INSTANCE.getModel(desc) != nullptr) {
            count++;
            desc = dd + "-" + std::to_string(count);
        }
        PixelOverlayManager::INSTANCE.addAutoOverlayModel(desc,
                                                          m_startChannel, m_channelCount, 3,
                                                          "H", m_invertedData ? "BL" : "TL",
                                                          m_height, 1);
        m_autoCreatedModelName = desc;
    }

    return ChannelOutput::Init(config);
}

bool BBShiftPanelOutput::usesOutput(int o) const {
    return m_panelMatrix && o >= 0 && o < MAX_MATRIX_OUTPUTS &&
           !m_panelMatrix->m_outputPanels[o].empty();
}

// The settings below describe how the PRU shifts a frame out, so they belong
// to the cape rather than to any one matrix on it.  The first matrix to
// register fixes them; checkCompatible() holds later ones to the same values.
BBShiftPanelManager::PanelParams BBShiftPanelManager::parsePanelParams(const Json::Value& config, const Json::Value& capeConfig) {
    PanelParams p;
    p.panelWidth = config["panelWidth"].asInt();
    p.panelHeight = config["panelHeight"].asInt();
    if (!p.panelWidth) {
        p.panelWidth = 32;
    }
    if (!p.panelHeight) {
        p.panelHeight = 16;
    }

    p.addressingMode = config["panelRowAddressType"].asInt();
    p.panelType = config["panelType"].asInt();
    // For PWM panel types the addressing dropdown selects how the GCLK
    // program drives the row lines: Direct Row Select = binary row number
    // on SEL0-4, anything else = the DP32020A style row shift register
    p.pwmDirectRow = (p.addressingMode == ADDRESSING_MODE_DIRECT);
    if (p.panelType == PANEL_TYPE_FM6363C) {
        // the UI moved FM6363C from the addressing dropdown to the panel
        // type dropdown; internally it stays the PWM addressing mode (old
        // configs with panelRowAddressType == 51 keep working unchanged)
        p.addressingMode = ADDRESSING_MODE_FM6363C;
        p.panelType = 0;
    } else if (p.panelType == PANEL_TYPE_FM6353) {
        p.addressingMode = ADDRESSING_MODE_FM6353C;
        p.panelType = 0;
    } else if (p.panelType == PANEL_TYPE_FM6373) {
        p.addressingMode = ADDRESSING_MODE_FM6373;
        p.panelType = 0;
    } else if (p.panelType == PANEL_TYPE_DP3364) {
        p.addressingMode = ADDRESSING_MODE_DP3364;
        p.panelType = 0;
    } else if (p.panelType == PANEL_TYPE_ICND1065L) {
        p.addressingMode = ADDRESSING_MODE_ICND1065L;
        p.panelType = 0;
    } else if (p.panelType == PANEL_TYPE_SM16380SH) {
        p.addressingMode = ADDRESSING_MODE_SM16380SH;
        p.panelType = 0;
    } else if (p.panelType == PANEL_TYPE_ICND2153) {
        p.addressingMode = ADDRESSING_MODE_ICND2153;
        p.panelType = 0;
    }
    bool pwm = p.addressingMode >= ADDRESSING_MODE_FM6353C;

    // A combo cape shares the PRUSS with a string driver on the other PRU:
    // panels run single-PRU with the smaller split shared-memory ring and
    // must not touch anything the other driver owns.  (The flag is also
    // honored from the output config for bench testing.)
    p.sharedPRUSS = capeConfig["sharedPRUSS"].asBool() || config["sharedPRUSS"].asBool();

    p.colorDepth = 12;
    if (config.isMember("panelColorDepth")) {
        p.colorDepth = config["panelColorDepth"].asInt();
    }
    p.outputByRow = false;
    p.outputBlankData = false;
    if (config.isMember("panelOutputOrder")) {
        p.outputByRow = config["panelOutputOrder"].asBool();
    }
    if (config.isMember("panelOutputBlankRow")) {
        p.outputBlankData = config["panelOutputBlankRow"].asBool();
    }
    if (p.colorDepth < 0) {
        p.colorDepth = -p.colorDepth;
        p.outputBlankData = true;
    }
    if (p.colorDepth > 12 || p.colorDepth < 6) {
        p.colorDepth = 8;
    }
    if (pwm) {
        p.colorDepth = 16;
    }

    p.panelInterleave = "";
    if (config.isMember("panelInterleave")) {
        p.panelInterleave = config["panelInterleave"].asString();
    }
    p.panelScan = config["panelScan"].asInt();
    if (p.panelScan == 0) {
        //  default scan is 1/2 the height of the panel
        p.panelScan = p.panelHeight / 2;
    }

    // Full height data layouts.  Only meaningful when every row has its own
    // scan address, because that is what frees the second RGB lane to carry
    // the other half of the same row instead of the other half of the panel.
    p.dataLayout = config["panelDataLayout"].asInt();
    if (p.dataLayout) {
        if (p.panelScan != p.panelHeight) {
            LogErr(VB_CHANNELOUT, "BBShiftPanel: full height data layout needs scan (%d) to equal panel height (%d); using the standard layout\n",
                   p.panelScan, p.panelHeight);
            WarningHolder::AddWarning("LED panel data layout requires the scan rate to equal the panel height");
            p.dataLayout = 0;
        } else if (p.panelWidth & 1) {
            LogErr(VB_CHANNELOUT, "BBShiftPanel: full height data layout needs an even panel width (%d); using the standard layout\n",
                   p.panelWidth);
            p.dataLayout = 0;
        } else if (!p.panelInterleave.empty()) {
            LogErr(VB_CHANNELOUT, "BBShiftPanel: full height data layout cannot be combined with panel interleave; using the standard layout\n");
            WarningHolder::AddWarning("LED panel data layout cannot be combined with panel interleave");
            p.dataLayout = 0;
        }
    }
    return p;
}

bool BBShiftPanelManager::parseCapeConfig(const Json::Value& root) {
    // Mux only the pins the cape actually uses: the named control pins plus
    // the data pins its outputs reference.  On the AM62x every PRU1 pin can
    // alternatively be muxed to PRU0, so a cape that lists fewer outputs
    // leaves the unreferenced data pins available for another driver (e.g.
    // BBShiftString on the other PRU) - the PRU still writes all 8 data
    // bits of r30, writes to unmuxed bits just never reach a pin.  Capes
    // without a controls section get the full historical pin set.
    if (root.isMember("controls") && root["controls"].size()) {
        std::set<std::string> pinNames;
        for (auto const& name : root["controls"].getMemberNames()) {
            pinNames.insert(root["controls"][name]["pin"].asString());
        }
        for (int i = 0; i < (int)root["outputs"].size(); i++) {
            pinNames.insert(root["outputs"][i]["pin"].asString());
        }
        m_configuredPins.assign(pinNames.begin(), pinNames.end());
    } else {
        m_configuredPins = PRU_PINS;
    }
    for (auto& pinName : m_configuredPins) {
        PinCapabilities::getPinByName(pinName).configPin("pru1out", true);
    }
    if (root["controls"].isMember("oe")) {
        m_oePin = root["controls"]["oe"]["pin"].asString();
    }
    m_numOutputs = root["outputs"].size();
    if (m_numOutputs > MAX_OUTPUTS) {
        m_numOutputs = MAX_OUTPUTS;
    }
    m_numOutputSlots = 8;
    for (int i = 0; i < m_numOutputs; i++) {
        Json::Value s = root["outputs"][i];
        std::string pinName = s["pin"].asString();
        const BBBPinCapabilities* pin = (const BBBPinCapabilities*)(PinCapabilities::getPinByName(pinName).ptr());
        outputPin[i] = pin->pruPin(1);
        outputBank[i] = 0;
        if (s.isMember("index")) {
            outputBank[i] = s["index"].asInt();
            if (outputBank[i] > 0) {
                m_numOutputSlots = 16;
            }
        }
    }
    if (root.isMember("singlePRU")) {
        singlePRU = root["singlePRU"].asBool();
    }
    // singlePRU = true;
    if (!singlePRU) {
        // if not using a single PRU, then we need to change the OE pin to the other PRU
        const PinCapabilities& pin = PinCapabilities::getPinByName(m_oePin);
        pin.configPin("pru0out", true);
    }
    return true;
}

bool BBShiftPanelManager::adoptPanelParams(const PanelParams& p) {
    m_panelWidth = p.panelWidth;
    m_panelHeight = p.panelHeight;
    m_panelScan = p.panelScan;
    m_panelInterleave = p.panelInterleave;
    m_addressingMode = p.addressingMode;
    m_panelType = p.panelType;
    m_dataLayout = p.dataLayout;
    m_pwmDirectRow = p.pwmDirectRow;
    m_colorDepth = p.colorDepth;
    m_outputByRow = p.outputByRow;
    m_outputBlankData = p.outputBlankData;
    m_sharedPRUSS = p.sharedPRUSS;

    if (m_sharedPRUSS) {
        if (isPWMPanel()) {
            LogErr(VB_CHANNELOUT, "PWM panel types require both PRUs and cannot be used on a shared panels+strings cape\n");
            WarningHolder::AddWarning("PWM panel types cannot be used on a shared panels+strings cape");
            return false;
        }
        singlePRU = true;
    }

    if (isPWMPanel()) {
        for (auto& pinName : PRU0_PWM_PINS) {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            pin.configPin("pru0out", true);
        }
    }
    return true;
}

// A second matrix on the cape shares one frame, one stride schedule and one
// set of PRU firmware, so anything that shapes those has to be identical.  The
// per-matrix settings (gamma, color order, start corner, brightness, layout)
// are deliberately not in here.
bool BBShiftPanelManager::checkCompatible(const PanelParams& p) const {
    const char* bad = nullptr;
    if (p.panelWidth != m_panelWidth || p.panelHeight != m_panelHeight) {
        bad = "panel size";
    } else if (p.panelScan != m_panelScan) {
        bad = "panel scan";
    } else if (p.panelInterleave != m_panelInterleave) {
        bad = "panel interleave";
    } else if (p.addressingMode != m_addressingMode || p.panelType != m_panelType) {
        bad = "panel type / row addressing";
    } else if (p.dataLayout != m_dataLayout) {
        bad = "panel data layout";
    } else if (p.colorDepth != m_colorDepth) {
        bad = "color depth";
    } else if (p.outputByRow != m_outputByRow || p.outputBlankData != m_outputBlankData) {
        bad = "output order / blank row";
    }
    if (bad) {
        std::string w = std::string("All LED panel matrices on one cape must use the same ") + bad +
                        "; this matrix was not started.";
        LogErr(VB_CHANNELOUT, "BBShiftPanel: %s\n", w.c_str());
        WarningHolder::AddWarning(w);
        return false;
    }
    return true;
}

bool BBShiftPanelManager::addMember(BBShiftPanelOutput* m, const Json::Value& config, const Json::Value& capeConfig) {
    PanelParams p = parsePanelParams(config, capeConfig);
    if (m_members.empty()) {
        if (!parseCapeConfig(capeConfig) || !adoptPanelParams(p)) {
            return false;
        }
    } else if (!checkCompatible(p)) {
        return false;
    }

    // Each output is one set of byte lanes in the shared frame, so two
    // matrices cannot drive the same one - they would each be writing the
    // other's pixels.
    for (int o = 0; o < m_numOutputs; o++) {
        if (!m->usesOutput(o)) {
            continue;
        }
        for (auto* other : m_members) {
            if (other->usesOutput(o)) {
                std::string w = "Two LED panel matrices are both configured to use cape output " +
                                std::to_string(o + 1) + "; the second matrix was not started.";
                LogErr(VB_CHANNELOUT, "BBShiftPanel: %s\n", w.c_str());
                WarningHolder::AddWarning(w);
                return false;
            }
        }
    }

    m_members.push_back(m);
    ++m_openMembers;
    if (!rebuild()) {
        m_members.pop_back();
        --m_openMembers;
        if (m_members.empty()) {
            // nothing is left holding the hardware, so do not leave the
            // half-built frame buffers allocated
            teardown();
        }
        return false;
    }
    return true;
}

// The frame is as wide as the longest chain on the cape and as tall as one
// panel's scan requires; every member maps into that same shape.
void BBShiftPanelManager::computeGeometry() {
    numRows = 0;
    maxRowLen = 0;
    // cleared up front so a member set with no panels leaves a zero sized
    // frame rather than the previous rebuild's geometry
    rowLen = 0;

    bool anyPanels = false;
    m_longestChain = 0;
    m_brightness = 1;
    for (auto* m : m_members) {
        if (m->m_panels > 0) {
            anyPanels = true;
        }
        if (m->longestChain() > m_longestChain) {
            m_longestChain = m->longestChain();
        }
        // the OE on-time is shared, so the cape runs at the brightest setting
        // any matrix asked for and the others scale their gamma down to it
        if (m->configuredBrightness() > m_brightness) {
            m_brightness = m->configuredBrightness();
        }
    }
    if (!anyPanels) {
        return;
    }

    if (m_dataLayout) {
        // Full height: every row has its own scan address, and the two RGB
        // lanes split that row down the middle rather than splitting the
        // panel top from bottom.  Interleave is rejected in parsePanelParams
        // for this layout, so there is no handler to consult.
        numRows = m_panelHeight;
        maxRowLen = m_panelWidth / 2;
        rowLen = maxRowLen * m_longestChain;
        return;
    }

    PanelInterleaveHandler* handler = PanelInterleaveHandler::createHandler(m_panelInterleave, m_panelWidth, m_panelHeight, m_panelScan);
    if (!handler) {
        LogErr(VB_CHANNELOUT, "Failed to create panel interleave handler\n");
        return;
    }
    // the mapping only depends on the position within a panel, so one pass
    // over a single panel gives the row count and row length for all of them
    for (int y = 0; y < (m_panelHeight / 2); y++) {
        for (int x = 0; x < m_panelWidth; ++x) {
            int yOut = y;
            int xOut = x;
            handler->map(xOut, yOut);
            if (yOut >= (int)numRows) {
                numRows = yOut + 1;
            }
            if (xOut >= (int)maxRowLen) {
                maxRowLen = xOut + 1;
            }
        }
    }
    delete handler;
    rowLen = maxRowLen * m_longestChain;
}

// Brings the shared hardware up around the current member set.  Called for
// every registration, so adding a matrix with a longer chain re-maps the ones
// already registered onto the new frame width.
bool BBShiftPanelManager::rebuild() {
    teardownHardware();
    computeGeometry();

    uint32_t dataSize = intermediateSize();
    if (!dataSize) {
        LogErr(VB_CHANNELOUT, "BBShiftPanel: no panels configured, nothing to output\n");
        return false;
    }
    if (currentChannelData) {
        delete[] currentChannelData;
    }
    currentChannelData = new uint16_t[dataSize];
    memset(currentChannelData, 0, dataSize * sizeof(uint16_t));

    for (auto* m : m_members) {
        m->setupGamma();
        if (!m->buildScatterMap()) {
            return false;
        }
    }

    pru = new BBBPru(1, true, true);
    pruData = (BBShiftPanelData*)pru->data_ram;
    if (!isPWMPanel()) {
        buildStrideSchedule();
    }
    if (StartPRU() == 0) {
        return false;
    }
    if (isPWMPanel()) {
        setupPWMRegisters();
    } else {
        setupBrightnessValues();
    }
    startBackgroundThreads();
    return true;
}

void BBShiftPanelManager::startBackgroundThreads() {
    if (bgThreadsRunning) {
        return;
    }
    bgThreadsRunning = true;
    for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
        std::thread th(&BBShiftPanelManager::runBackgroundTasks, this);
        th.detach();
    }
}

void BBShiftPanelManager::stopBackgroundThreads() {
    if (!bgThreadsRunning) {
        return;
    }
    bgThreadsRunning = false;
    bgTaskCondVar.notify_all();
    while (bgThreadCount > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int BBShiftPanelManager::StartPRU() {
    if (!pru) {
        pru = new BBBPru(1, true, true);
        pruData = (BBShiftPanelData*)pru->data_ram;
    }
    bool started = true;
    if (isPWMPanel()) {
        pwmPru = new BBBPru(0);
        started &= pwmPru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_gclk.out");
        if (m_numOutputSlots == 16) {
            started &= pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_pwm_16.out");
        } else {
            started &= pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_pwm.out");
        }
    } else {
        if (singlePRU) {
            // on a shared cape the string driver owns the shared RAM half
            // and the other PRU - never clear them from here
            if (m_numOutputSlots == 16) {
                started &= pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_single_16.out", !m_sharedPRUSS);
            } else {
                started &= pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_single.out", !m_sharedPRUSS);
            }
        } else {
            // in the two-PRU shift configuration the OE PRU also prefetches
            // the ring data and hands it over through the scratchpad, which
            // shortens the main PRU's per-block load considerably
            if (m_numOutputSlots == 16) {
                started &= pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_prefetch_16.out");
            } else {
                started &= pru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_prefetch.out");
            }
            pwmPru = new BBBPru(0);
            started &= pwmPru->run("/opt/fpp/src/non-gpl/BBShiftPanel/BBShiftPanel_oe_prefetch.out");
        }
    }
    if (!started) {
        LogErr(VB_CHANNELOUT, "BBShiftPanel: Unable to start PRU(s). May require a reboot.\n");
        WarningHolder::AddWarning("BBShiftPanel: Unable to start PRU(s). May require a reboot.");
        return 0;
    }
    // The same data RAM word configures either firmware: the shift firmware
    // reads its row addressing config right after the ring config handshake,
    // the PWM firmware reads the chip family flag at each register upload.
    // It must land after run() (the firmware load clears the PRU memories)
    // and, for the shift firmware, before the ring attach below.
    uint32_t addrCfg;
    if (isPWMPanel()) {
        // b0 = chip family, b1 = register slot count, b2 = middle LAT burst
        // length (0 = skip it).  Only the FM6373 family reads b1/b2.
        if (const PWMChipSeq* seq = pwmChipSeqFor(m_addressingMode)) {
            addrCfg = 1 | ((uint32_t)seq->slots << 8) | ((seq->midLatch ? 11u : 0u) << 16);
        } else if (m_addressingMode == ADDRESSING_MODE_DP3364) {
            // The DP3364S upload is the same shape as the FM6373 one with a
            // single register slot and no middle LAT burst: VSYNC (LE 3),
            // PRE_ACT (LE 14), then one word latched over its last 5 clocks.
            // Sharing that code rather than duplicating it matters - the
            // 16 output firmware only just fits in the PRU's 12KB IMEM.
            addrCfg = 1 | (1u << 8) | (0u << 16);
        } else {
            addrCfg = 0;
        }
    } else {
        addrCfg = (uint32_t)(m_addressingMode & 0xFF) | (((uint32_t)numRows) << 8);
    }
    *(volatile uint32_t*)(pru->data_ram + ADDR_CONFIG_OFFSET) = addrCfg;
    __sync_synchronize();
    // Both panel types are fed through a ring in the PRU shared memory (see
    // SMEMRing.hp); attach() must happen after run() since the firmware load
    // clears the PRU memories (and writes while the PRUSS is powered down
    // are silently dropped).  A sole owner gets the entire shared RAM; on a
    // shared cape the panels are the PRU1 consumer and take the SPLIT1 half
    // (BBShiftString's PRU0 program uses SPLIT0, matching its existing
    // per-PRU split assignment).
    if (m_sharedPRUSS) {
        m_ring.attach(pru, SMEM_RING_SPLIT1_BASE, SMEM_RING_SPLIT_SIZE, false);
    } else {
        m_ring.attach(pru, SMEM_RING_DEFAULT_BASE, SMEM_RING_DEFAULT_SIZE, false);
    }
    uint32_t bytesPerPixel = (m_numOutputSlots * 6 * 2) / 16;
    uint32_t strideLen = rowLen * bytesPerPixel;
    uint32_t numStride = m_strideSchedule.empty() ? (numRows * m_colorDepth) : (m_strideSchedule.size() * numRows);
    uint32_t oframeSize = numStride * strideLen;
    // PWM panels consume numRows passes of 16 PWM bit planes of numBlocks
    // (rowLen / 16) blocks of 16 pixels, which works out to the same full
    // frame the pixel mapping produces
    m_frameBytes = oframeSize;
    if (!isPWMPanel()) {
        pruData->numStrides = numStride;
    }
    // Frames live in normal cached memory now that the pump thread streams
    // them to the PRU; the prep threads write cached memory much faster than
    // the uncached PRU DDR region this replaced
    uint32_t allocSize = (oframeSize + 4095) & ~4095;
    for (int x = 0; x < NUM_OUTPUT_BUFFERS; x++) {
        outputBuffers[x] = (uint8_t*)aligned_alloc(4096, allocSize);
        memset(outputBuffers[x], 0, allocSize);
    }
    m_heapBuffers = true;
    m_frontBuffer = nullptr;
    // Reset the handshake counters with it.  These outlive the pump thread, so
    // on a restart (teardownHardware() then back through here) stale values
    // would leave the new thread thinking a frame from the previous
    // configuration is still owed, and SendData waiting on a priming that
    // already happened.
    m_pumpSeq.store(0, std::memory_order_relaxed);
    m_pumpedSeq = 0;
    m_pumpPrimed.store(0, std::memory_order_relaxed);
    m_pumpRunning = true;
    m_pumpThread = std::thread(&BBShiftPanelManager::runPumpThread, this);
    return 1;
}

void BBShiftPanelManager::runPumpThread() {
    struct sched_param sp;
    sp.sched_priority = 50;
    int err = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (err) {
        LogWarn(VB_CHANNELOUT, "BBShiftPanel: could not set pump thread to SCHED_FIFO: %s\n", strerror(err));
    }
    if (isPWMPanel()) {
        // PWM panels send one frame per DATA command (the panels refresh
        // themselves from their internal PWM); stream once per queued
        // command so the byte flow stays in step with the commands
        while (m_pumpRunning) {
            if (m_pumpedSeq == m_pumpSeq.load(std::memory_order_acquire)) {
                // PrepDataPWM wakes this the moment a frame is published; the
                // timeout is only a backstop so shutdown cannot wedge here.
                // Polling on a 500us sleep used to leave the PRU shifting
                // against an empty ring for most of that wakeup latency.
                std::unique_lock<std::mutex> lk(m_pumpMutex);
                m_pumpCV.wait_for(lk, std::chrono::microseconds(200), [this] {
                    return !m_pumpRunning || m_pumpSeq.load(std::memory_order_acquire) != m_pumpedSeq;
                });
                continue;
            }
            ++m_pumpedSeq;
            uint8_t* src = m_frontBuffer.load(std::memory_order_acquire);
            if (!src) {
                // The sequence can be ahead of the buffer if a reconfigure
                // republished it while a frame was in flight; there is nothing
                // to stream, and the shift path below makes the same check.
                m_pumpPrimed.store(m_pumpedSeq, std::memory_order_release);
                continue;
            }
            uint32_t srcOff = 0;
            bool primed = false;
            while (srcOff < m_frameBytes && m_pumpRunning) {
                uint32_t n = m_ring.write(src + srcOff, std::min(PUMP_BLOCK_SIZE, m_frameBytes - srcOff));
                if (n == 0) {
                    // the ring holds as much as it can - the PRU may start.
                    // Back off in small steps: at the drain rate a 150us sleep
                    // was long enough to open a gap the panel could see.
                    if (!primed) {
                        primed = true;
                        m_pumpPrimed.store(m_pumpedSeq, std::memory_order_release);
                    }
                    struct timespec ts = { 0, 20000 };
                    nanosleep(&ts, nullptr);
                    continue;
                }
                srcOff += n;
            }
            // a frame smaller than the ring never fills it
            if (!primed) {
                m_pumpPrimed.store(m_pumpedSeq, std::memory_order_release);
            }
        }
        return;
    }
    while (m_pumpRunning) {
        if (m_pumpPaused.load(std::memory_order_acquire)) {
            // Nothing is being output, so there is nothing to hold on the
            // panel.  Clear the command word and stop: the PRU picks it up at
            // the end of the frame it is in and parks in its command-wait
            // loop, which holds the display off, and the OE PRU parks the
            // same way once the brightness handshake stops.  SendData writes
            // the command back on the first frame after StartingOutput.
            //
            // This is only reached at a frame boundary, and the ring is much
            // smaller than one frame, so the PRU has at most the tail of the
            // frame just written left to consume.  It ends up exactly where
            // the pump stopped and the two stay aligned in the byte stream
            // across the pause.
            if (!m_pumpParked.load(std::memory_order_relaxed)) {
                pruData->pixelsPerStride = 0;
                __sync_synchronize();
                m_pumpParked.store(true, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        m_pumpParked.store(false, std::memory_order_relaxed);
        // frames stream back to back; a new frame from SendData is picked up
        // at the frame boundary so the PRU always sees whole frames
        uint8_t* src = m_frontBuffer.load(std::memory_order_acquire);
        if (!src) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        uint32_t srcOff = 0;
        while (srcOff < m_frameBytes && m_pumpRunning) {
            uint32_t n = m_ring.write(src + srcOff, std::min(PUMP_BLOCK_SIZE, m_frameBytes - srcOff));
            if (n == 0) {
                // ring full; a full ring lasts ~480us at the max drain rate
                // so a short sleep cannot underrun the PRU
                struct timespec ts = { 0, 150000 };
                nanosleep(&ts, nullptr);
                continue;
            }
            srcOff += n;
        }
    }
}
void BBShiftPanelManager::StopPRU(bool wait) {
    // A halted core cannot see the stop command, and a parked one is already
    // sitting on the check, so let them run first
    UnparkPRUs();
    // Send the stop command
    if (pru) {
        pruData->command = PWM_COMMAND_HALT;
        pruData->result = PWM_COMMAND_HALT;
    }
    __asm__ __volatile__("" ::
                             : "memory");

    if (pru) {
        int cnt = 0;
        // the pump keeps feeding the ring here so the PRU can reach the end
        // of the frame, which is where the halt command is checked
        while (wait && cnt < 25 && pruData->result == PWM_COMMAND_HALT) {
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
    if (pru) {
        pru->stop();
        delete pru;
        pru = nullptr;

        if (pwmPru) {
            pwmPru->stop();
            delete pwmPru;
            pwmPru = nullptr;
        }
    }
}

int BBShiftPanelOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::Close()\n");
    if (!m_autoCreatedModelName.empty()) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(m_autoCreatedModelName);
        m_autoCreatedModelName.clear();
    }
    releaseToManager();
    return ChannelOutput::Close();
}

void BBShiftPanelOutput::releaseToManager() {
    if (m_registered && !m_closed) {
        m_closed = true;
        BBShiftPanelManager::INSTANCE.closeMember();
    }
}

void BBShiftPanelManager::closeMember() {
    if (m_openMembers > 0 && --m_openMembers) {
        // the pins stay muxed while another matrix on the cape still needs them
        return;
    }
    if (!m_refreshWarning.empty()) {
        WarningHolder::RemoveWarning(m_refreshWarning);
        m_refreshWarning.clear();
    }
    // release only the pins this cape muxed; other drivers (a future
    // panels + strings combo cape) may own the rest
    for (auto& pinName : m_configuredPins) {
        const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
        pin.releasePin();
    }
    if (!singlePRU) {
        // if not using a single PRU, then we need to change the OE pin to the other PRU
        const PinCapabilities& pin = PinCapabilities::getPinByName(m_oePin);
        pin.releasePin();
    }
    if (isPWMPanel()) {
        for (auto& pinName : PRU0_PWM_PINS) {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            pin.releasePin();
        }
    }
    m_configuredPins.clear();
}

void BBShiftPanelManager::runBackgroundTasks() {
    ++bgThreadCount;
    while (bgThreadsRunning) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(bgTaskMutex);
            bgTaskCondVar.wait_for(lock, std::chrono::milliseconds(100), [&]() { return !bgTasks.empty(); });
            if (!bgTasks.empty()) {
                task = bgTasks.front();
                bgTasks.pop();
            }
        }
        if (task) {
            task();
        }
    }
    --bgThreadCount;
}

void BBShiftPanelManager::processTasks(std::atomic<int>& counter) {
    // This must always drain to zero, even during shutdown: the queued tasks
    // reference the calling frame's stack (counter, results) and this object,
    // so returning while any are queued or running leaves them with dangling
    // references.  The pool threads exit without draining the queue, but this
    // loop executes leftover tasks itself so it cannot deadlock.
    std::unique_lock<std::mutex> lock(bgTaskMutex);
    while (counter > 0) {
        std::function<void()> task;
        if (!bgTasks.empty()) {
            task = bgTasks.front();
            bgTasks.pop();
        }
        lock.unlock();
        if (task) {
            task();
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        lock.lock();
    }
}

void BBShiftPanelOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftPanelOutput::PrepData(%p)\n", channelData);
    if (!m_registered) {
        return;
    }
    ++m_inFlight;
    BBShiftPanelManager::INSTANCE.prepMember(this, channelData);
    --m_inFlight;
}

// Scatters this matrix's channels into its own byte lanes of the shared 16 bit
// intermediate.  Members never touch each other's lanes, so this is safe to run
// for each matrix in turn with no coordination beyond the frame boundary below.
void BBShiftPanelOutput::scatterFrame(unsigned char* channelData) {
    auto& mgr = BBShiftPanelManager::INSTANCE;
    uint16_t* dest = mgr.currentChannelData;
    if (!dest) {
        return;
    }
    m_matrix->OverlaySubMatrices(channelData);
    channelData += m_startChannel;

    std::unique_lock<std::mutex> lock(mgr.bgTaskMutex);
    size_t total = m_scatterOffsets.size();
    size_t start = 0;
    std::atomic<int> counter(0);
    while (start < total) {
        // each chunk writes a contiguous region of the output since the map
        // is sorted by destination
        size_t end = std::min(start + 64 * 1024, total);
        ++counter;
        mgr.bgTasks.push([this, channelData, start, end, dest, &counter]() {
            const uint32_t* offs = m_scatterOffsets.data();
            const uint32_t* srcs = m_scatterSrc.data();
            for (size_t x = start; x < end; x++) {
                dest[offs[x]] = gammaCurve[channelData[srcs[x]]];
            }
            --counter;
        });
        start = end;
    }
    lock.unlock();
    mgr.bgTaskCondVar.notify_all();
    mgr.processTasks(counter);
}

// One frame's worth of work for the whole cape.  Every member scatters into
// the shared intermediate first; the bit-plane pass is expensive and produces
// a single frame for all of them, so it runs once, when the last member has
// arrived.  Doing it here rather than in SendData keeps it out of the way of
// the other outputs' wire sends.
void BBShiftPanelManager::prepMember(BBShiftPanelOutput* m, unsigned char* channelData) {
    if (!m->m_stopping) {
        m->scatterFrame(channelData);
    }
    // A stopping member contributes nothing but must still be counted, or the
    // frame boundary would never be reached again and output would stall.
    if (++m_preppedThisFrame < (int)m_members.size()) {
        return;
    }
    m_preppedThisFrame = 0;
    if (!bgThreadsRunning) {
        return;
    }
    if (isPWMPanel()) {
        PrepDataPWM();
    } else {
        PrepDataShift();
    }
    m_framePrepped = true;
}

void BBShiftPanelManager::PrepDataPWM() {
    uint8_t* buf = outputBuffers[currOutputBuffer];

    std::unique_lock<std::mutex> lock(bgTaskMutex);
    std::atomic<int> counter(0);
    for (int curRow = 0; curRow < numRows; curRow++) {
        // Map the pixels for this row
        ++counter;
        bgTasks.push([this, curRow, buf, &counter]() {
            uint32_t start = curRow * rowLen;
            uint32_t end = start + rowLen;

            if (m_numOutputSlots == 16) {
                ispc::MapPixelsForPWM16(currentChannelData, start, end, (uint16_t*)buf);
            } else {
                ispc::MapPixelsForPWM(currentChannelData, start, end, (uint16_t*)buf);
            }
            --counter;
        });
    }
    lock.unlock();
    bgTaskCondVar.notify_all();
    processTasks(counter);

    /*
    for (int x = 0; x < 48; x++) {
        printf("%04x  ", currentChannelData[x]);
    }
    printf("\n");
    for (int x = 0; x < 96; x += 6) {
        printf("%02x %02x %02x %02x %02x %02x\n", buf[x], buf[x + 1], buf[x + 2], buf[x + 3], buf[x + 4], buf[x + 5]);
    }
    printf("\n");
    for (int x = 48; x < 96; x++) {
        printf("%04x  ", currentChannelData[x]);
    }
    printf("\n");
    for (int x = 96; x < (96 * 2); x += 6) {
        printf("%02x %02x %02x %02x %02x %02x\n", buf[x], buf[x + 1], buf[x + 2], buf[x + 3], buf[x + 4], buf[x + 5]);
    }
    printf("\n");
    */

    // wait for the PRU to take the previous command before queueing the next
    while (pruData->command && bgThreadsRunning) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        __asm__ __volatile__("" ::
                                 : "memory");
    }

    // hand the frame to the pump thread
    m_frontBuffer.store(buf, std::memory_order_release);
    uint32_t seq = m_pumpSeq.fetch_add(1, std::memory_order_release) + 1;
    m_pumpCV.notify_one();
    // Hold the DATA command until the ring is primed.  Starting the PRU
    // against an empty ring left it in RINGWAIT for the pump's wakeup
    // latency, and an FM6363C reads that gap as its row scan running away
    // from the data.  Filling the ring takes ~65us; the loop bound is only a
    // backstop so a stopped pump cannot hang the output thread.
    for (int i = 0; i < 4000 && m_pumpRunning && bgThreadsRunning &&
                    m_pumpPrimed.load(std::memory_order_acquire) != seq;
         ++i) {
        std::this_thread::yield();
    }

    pruData->numBlocks = rowLen / 16;
    pruData->numRows = numRows;
    pruData->cmd = PWM_COMMAND_DATA;
    __asm__ __volatile__("" ::
                             : "memory");
}

void BBShiftPanelManager::PrepDataShift() {
    std::array<std::array<uint16_t*, 16>, 32> results;

    uint32_t bytesPerPixel = (m_numOutputSlots * 6 * 2) / 16;
    uint32_t strideLen = rowLen * bytesPerPixel;
    uint8_t* base = outputBuffers[currOutputBuffer];
    int numSlots = m_strideSchedule.size();
    for (int s = 0; s < numSlots; s++) {
        if (!m_strideSchedule[s].primary) {
            continue;
        }
        int b = m_strideSchedule[s].bit;
        for (int r = 0; r < numRows; r++) {
            uint32_t idx = m_outputByRow ? (r * numSlots + s) : (s * numRows + r);
            results[r][b] = (uint16_t*)(base + idx * strideLen);
        }
    }

    /*
        int len = rowLen * numRows * 6 * 8
        uint16_t *d = data;
        for (int x = 0; x < len; x += 8) {
            for (int y = 0; y < 8; y++) {
                uint16_t d2 = *d;
                uint16_t mask = 0x1;
                uint8_t bit = 0x1 << y;
                for (int pos = 0; pos < bits; pos++) {
                    if (d2 & mask) {
                        results[pos][x] |= bit;
                    }
                    mask <<= 1;
                }
                ++d;
            }
        }
    */
    // Use ISPC generated code for the above.  It's about 9x faster
    std::unique_lock<std::mutex> lock(bgTaskMutex);
    std::atomic<int> counter(0);
    for (int curRow = 0; curRow < numRows; curRow++) {
        // Map the pixels for this row
        ++counter;
        bgTasks.push([this, curRow, strideLen, base, &results, &counter]() {
            uint32_t start = curRow * rowLen * 6 * m_numOutputSlots;
            uint32_t end = start + (rowLen * 6 * m_numOutputSlots);
            ispc::MapPixelsByDepth16(currentChannelData, start, end, m_colorDepth,
                                     results[curRow][0], results[curRow][1],
                                     results[curRow][2], results[curRow][3],
                                     results[curRow][4], results[curRow][5],
                                     results[curRow][6], results[curRow][7],
                                     results[curRow][8], results[curRow][9],
                                     results[curRow][10], results[curRow][11],
                                     results[curRow][12], results[curRow][13],
                                     results[curRow][14], results[curRow][15]);
            // fill in the duplicated strides for bits that display as multiple pulses
            for (auto& dup : m_dupCopies[curRow]) {
                memcpy(base + dup.first, base + dup.second, strideLen);
            }
            --counter;
        });
    }
    lock.unlock();
    bgTaskCondVar.notify_all();
    processTasks(counter);
}

int BBShiftPanelOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "BBShiftPanelOutput::SendData(%p)\n", channelData);
    if (!m_registered || m_stopping) {
        return m_channelCount;
    }
    ++m_inFlight;
    BBShiftPanelManager::INSTANCE.publishFrame();
    --m_inFlight;
    return m_channelCount;
}

// Hands the frame the last PrepData built to the pump.  Every member calls
// this, but the frame is shared, so only the first call after a prep does
// anything - the rest are no-ops.  This is deliberately cheap: a pointer
// store and a couple of PRU words, nothing that would hold up the other
// outputs waiting to send.
void BBShiftPanelManager::publishFrame() {
    if (!m_framePrepped) {
        return;
    }
    m_framePrepped = false;

    if (!isPWMPanel()) {
        // hand the just-prepared frame to the pump thread; it picks it up at
        // its next frame boundary so the PRU always sees whole frames
        m_frontBuffer.store(outputBuffers[currOutputBuffer], std::memory_order_release);
        pruData->numStrides = m_strideSchedule.size() * numRows;
        pruData->pixelsPerStride = rowLen;
    } else {
        while (pruData->command && bgThreadsRunning) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            __asm__ __volatile__("" ::
                                     : "memory");
        }

        // Send the command to setup the registers
        pruData->numBlocks = rowLen / 16;
        pruData->numRows = numRows;
        if (const PWMChipSeq* seq = pwmChipSeqFor(m_addressingMode)) {
            // rotate the config sequence one word per frame as a continuous
            // refresh; the vsync is part of the FM6373 register upload so
            // PWM_COMMAND_SYNC is not set for this family
            writeFM6373SeqWord(m_pwmSeqIdx);
            m_pwmSeqIdx = (m_pwmSeqIdx + 1) % seq->len;
            pruData->cmd = PWM_COMMAND_REGISTERS | PWM_COMMAND_STARTGCLK;
        } else if (m_addressingMode == ADDRESSING_MODE_DP3364) {
            // same one-register-per-frame rotation; the datasheet frames it
            // as the intended way to refresh all 15 registers without paying
            // for a full upload every frame.  The VSYNC is the first LE pulse
            // of the DP3364S register upload, so PWM_COMMAND_SYNC is not set.
            writeDP3364SeqWord(m_pwmSeqIdx);
            m_pwmSeqIdx = (m_pwmSeqIdx + 1) % DP3364_SEQ_LEN;
            pruData->cmd = PWM_COMMAND_REGISTERS | PWM_COMMAND_STARTGCLK;
        } else {
            pruData->cmd = PWM_COMMAND_REGISTERS | PWM_COMMAND_SYNC | PWM_COMMAND_STARTGCLK;
        }
    }

    __asm__ __volatile__("" ::
                             : "memory");
    currOutputBuffer = (currOutputBuffer + 1) % NUM_OUTPUT_BUFFERS;
    while (outputBuffers[currOutputBuffer] == nullptr) {
        currOutputBuffer = (currOutputBuffer + 1) % NUM_OUTPUT_BUFFERS;
    }
}
inline int mapRow(int row, int mode) {
    if (mode == ADDRESSING_MODE_DIRECT) {
        switch (row) {
        case 0:
            return 0x0E;
        case 1:
            return 0x0D;
        case 2:
            return 0x0B;
        case 3:
            return 0x07;
        }
    }
    return row;
}

static int outputRegData(int curidx, uint8_t* odata, uint16_t r, uint16_t g, uint16_t b, int numOutputs = 8) {
    // int sidx = curidx;
    int bytesPerClock = numOutputs == 16 ? 12 : 6;
    for (int x = 0; x < 16; x++) {
        odata[curidx] = r & 0x8000 ? 0xFF : 0x00;
        curidx++;
        odata[curidx] = g & 0x8000 ? 0xFF : 0x00;
        curidx++;
        odata[curidx] = b & 0x8000 ? 0xFF : 0x00;
        curidx++;
        odata[curidx] = r & 0x8000 ? 0xFF : 0x00;
        curidx++;
        odata[curidx] = g & 0x8000 ? 0xFF : 0x00;
        curidx++;
        odata[curidx] = b & 0x8000 ? 0xFF : 0x00;
        curidx++;
        if (numOutputs == 16) {
            // Duplicate for outputs 8-15
            odata[curidx] = r & 0x8000 ? 0xFF : 0x00;
            curidx++;
            odata[curidx] = g & 0x8000 ? 0xFF : 0x00;
            curidx++;
            odata[curidx] = b & 0x8000 ? 0xFF : 0x00;
            curidx++;
            odata[curidx] = r & 0x8000 ? 0xFF : 0x00;
            curidx++;
            odata[curidx] = g & 0x8000 ? 0xFF : 0x00;
            curidx++;
            odata[curidx] = b & 0x8000 ? 0xFF : 0x00;
            curidx++;
        }
        r <<= 1;
        g <<= 1;
        b <<= 1;
    }
    /*
    if (sidx == 0) {
        printf("%04X %04X %04X\n", r, g, b);
        for (int x = 0; x < 16; x++) {
            printf(" %02x %02x %02x %02x %02x %02x\n",
                   odata[sidx], odata[sidx + 1], odata[sidx + 2],
                   odata[sidx + 3], odata[sidx + 4], odata[sidx + 5]);
            sidx += 6;
        }
    }
    */
    return curidx;
}

void BBShiftPanelManager::setupPWMRegisters() {
    /*
    // FM6363 from colorlight + logic analyzer
    // Confirmed via "advanced" tab in LEDVision
    Reg 1: 0x0970:    b0000100101110000
    Reg 2: 0xFF9B  R: b1111111110011011  (default values)
           0xF39B  G: b1111001110011011
           0xDF9B  B: b1101111110011011
    Reg 2: 0xFE01  R: b1111110000000001 (with current stripped off)
           0xF201  G: b1111000000000001
           0xDE01  B: b1101110000000001
    Reg 3: 0x4007     b0100000000000111
    Reg 4: 0x0040     b0000000001000000
    Reg 5: 0x0000     b0000000000000000
    */
    static uint16_t conf_6363[] = {
        // R/G/B triplets
        0x0070, 0x0070, 0x0070,
        0xFC01, 0xF001, 0xDC01,
        0x4007, 0x4007, 0x4007,
        0x0040, 0x0040, 0x0040,
        0x0000, 0x0000, 0x0000
    };

    // create the "data" array of for all outputs of r/g/b triplets for the registers
    // 16 clocks * bytesPerClock * 5 registers
    int bytesPerClock = m_numOutputSlots == 16 ? 12 : 6;
    uint8_t odata[192 * 6];

    if (m_addressingMode == ADDRESSING_MODE_DP3364) {
        // DP3364S: one config register per frame, so only the single rotating
        // word slot is used (the VSYNC and PRE_ACT that precede it are bare LE
        // pulses with the data lines "not care" - see OUTPUT_REGISTERS_DP3364
        // in the asm).  Clock the whole sequence through before the first
        // frame, then SendData keeps rotating it as the continuous refresh the
        // datasheet asks for.
        setupGCLKConfig();
        pruData->numBlocks = rowLen / 16;
        pruData->numRows = numRows;
        m_pwmSeqIdx = 0;
        for (int i = 0; i < DP3364_SEQ_LEN; i++) {
            while (pruData->command) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                __asm__ __volatile__("" ::
                                         : "memory");
            }
            writeDP3364SeqWord(i);
            pruData->cmd = PWM_COMMAND_REGISTERS;
            __asm__ __volatile__("" ::
                                     : "memory");
            // the command word clears when the PRU *starts* the upload, so
            // give it time to finish before rewriting the rotating slot
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return;
    }

    if (const PWMChipSeq* seq = pwmChipSeqFor(m_addressingMode)) {
        // FM6373 family: the per-frame words are the 0x00AA/0x01AA
        // write-enable pair, one word of the config register sequence, an
        // optional chip specific word, and the 0x0055/0x0155 commit pair (see
        // OUTPUT_REGISTERS_FM6373 in the asm).  Load the fixed slots once,
        // then clock the entire sequence through the rotating slot so the
        // panel is fully configured before the first frame; SendData keeps
        // rotating it afterwards as a continuous refresh (DMD_STM32 and
        // kingdo9 both do the same).
        int idx = outputRegData(0, odata, 0x00AA, 0x00AA, 0x00AA, m_numOutputSlots);
        idx = outputRegData(idx, odata, 0x01AA, 0x01AA, 0x01AA, m_numOutputSlots);
        idx = outputRegData(idx, odata, seq->r[0], seq->g[0], seq->b[0], m_numOutputSlots);
        if (seq->slots == 6) {
            idx = outputRegData(idx, odata, seq->extraWord, seq->extraWord, seq->extraWord, m_numOutputSlots);
        }
        idx = outputRegData(idx, odata, 0x0055, 0x0055, 0x0055, m_numOutputSlots);
        idx = outputRegData(idx, odata, 0x0155, 0x0155, 0x0155, m_numOutputSlots);
        pru->memcpyToPRU((uint8_t*)&pruData->registers[0], &odata[0], idx);

        bool haveScan = ((int)numRows == seq->defaultScan);
        for (int v = 0; v < seq->variantCount && !haveScan; v++) {
            haveScan = (seq->variants[v].scan == (int)numRows);
        }
        if (!haveScan) {
            // These tables are per-capture, not per-chip: on this family the
            // current, subfield and tail registers move with the scan rate as
            // well as the scan count itself, so an unmatched rate is a
            // starting point rather than a correct configuration.
            LogWarn(VB_CHANNELOUT, "BBShiftPanel: no register table captured at 1/%u scan for this panel type; using the 1/%d table with only the scan count adjusted\n",
                    numRows, seq->defaultScan);
        }

        setupGCLKConfig();
        pruData->numBlocks = rowLen / 16;
        pruData->numRows = numRows;
        m_pwmSeqIdx = 0;
        for (int i = 0; i < seq->len; i++) {
            while (pruData->command) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                __asm__ __volatile__("" ::
                                         : "memory");
            }
            writeFM6373SeqWord(i);
            pruData->cmd = PWM_COMMAND_REGISTERS;
            __asm__ __volatile__("" ::
                                     : "memory");
            // the command word clears when the PRU *starts* the upload, so
            // give it time to finish before rewriting the rotating slot
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return;
    }

    // register 1 contains the number of scan lines (rows)
    uint16_t rn = ((numRows - 1) << 8) & 0x3F00;
    int curidx;
    if (m_addressingMode == ADDRESSING_MODE_ICND2153) {
        // ICND2153.  Same {4,6,8,10,2} latch grammar as the FM6353 below, and
        // the same 0x0070 base for the scan register, but its own payload -
        // from kingdo9/rpi-rgb-led-matrix_pwm_experiment (branch
        // icnd2153_bsparacino, SPWM_ICND2153_REGISTER_ENTRIES), captured from
        // a 64x32 1/8 scan panel.  Unlike the FM6353 this chip does carry
        // per-color current words in register 2.
        uint16_t r1 = 0x0070 | rn;
        curidx = outputRegData(0, odata, r1, r1, r1, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x7dfe, 0x71fe, 0x5dfe, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x4207, 0x4207, 0x4207, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x0040, 0x0040, 0x0040, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x0008, 0x0008, 0x0008, m_numOutputSlots);
    } else if (m_addressingMode == ADDRESSING_MODE_FM6353C) {
        // FM6353, values from DMD_STM32 (board707/DMD_STM32,
        // DMD_SPWM_Driver.h conf_6353 = {0x0008, 0x1f70, 0x6707, 0x40f7,
        // 0x0040} written in latch order 2,4,6,8,10).  The latch length
        // selects the register, so the 4,6,8,10,2 order the PRU sends
        // (REG1..REG5) writes the same registers.  No per-color current
        // data is known for this chip; brightness only comes from the
        // GCLK blanking time set below.
        uint16_t r1 = 0x0070 | rn;
        curidx = outputRegData(0, odata, r1, r1, r1, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x6707, 0x6707, 0x6707, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x40f7, 0x40f7, 0x40f7, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x0040, 0x0040, 0x0040, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, 0x0008, 0x0008, 0x0008, m_numOutputSlots);
    } else {
        curidx = outputRegData(0, odata, conf_6363[0] | rn, conf_6363[1] | rn, conf_6363[2] | rn, m_numOutputSlots);

        // register 2 contains adjustments for current

        uint16_t b = 205; // stick with default brightness for now
        if (m_brightness >= 5) {
            b = (m_brightness - 5) * 10;
            if (m_brightness > 8) {
                b *= (245 - 64) * 2;
            } else {
                b *= (205 - 64) * 2;
            }
            b /= 100;
            b += 64;
            b <<= 1;
            b &= 0x1FE;
            b |= 0x200;
        } else {
            b = (m_brightness - 1) * 10;
            b *= 100;
            b /= 40;
            b *= (255 - 64);
            b /= 100;
            b += 64;
            b <<= 1;
            b &= 0x1FE;
        }

        curidx = outputRegData(curidx, odata, conf_6363[3] | b, conf_6363[4] | b, conf_6363[5] | b, m_numOutputSlots);
        curidx = outputRegData(curidx, odata, conf_6363[6], conf_6363[7], conf_6363[8], m_numOutputSlots);
        curidx = outputRegData(curidx, odata, conf_6363[9], conf_6363[10], conf_6363[11], m_numOutputSlots);
        curidx = outputRegData(curidx, odata, conf_6363[12], conf_6363[13], conf_6363[14], m_numOutputSlots);
    }

    pru->memcpyToPRU(&pruData->registers[0], &odata[0], curidx);

    while (pruData->command) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        __asm__ __volatile__("" ::
                                 : "memory");
    }

    // Send the command to setup the registers
    setupGCLKConfig();
    pruData->numBlocks = rowLen / 16;
    pruData->numRows = numRows;
    pruData->cmd = PWM_COMMAND_REGISTERS;
    __asm__ __volatile__("" ::
                             : "memory");
}

void BBShiftPanelManager::writeFM6373SeqWord(int idx) {
    // rewrite the rotating register slot (slot 3 of 5) with sequence word
    // idx.  Only safe while no register upload is in flight: called from
    // SendData after the previous DATA command was consumed (the PRU is
    // uploading frame data, which never reads the register slots, and the
    // previous REGISTERS upload completed before that DATA was dispatched)
    // and from the serialized init loop in setupPWMRegisters.
    const PWMChipSeq* seq = pwmChipSeqFor(m_addressingMode);
    const uint16_t* sr = seq->r;
    const uint16_t* sg = seq->g;
    const uint16_t* sb = seq->b;
    for (int v = 0; v < seq->variantCount; v++) {
        if (seq->variants[v].scan == (int)numRows) {
            sr = seq->variants[v].r;
            sg = seq->variants[v].g;
            sb = seq->variants[v].b;
            break;
        }
    }
    uint16_t rw = sr[idx];
    uint16_t gw = sg[idx];
    uint16_t bw = sb[idx];
    if ((rw >> 8) == seq->scanReg) {
        // the scan row count lives in the low 6 bits; the upper 2 are a chip
        // constant carried over from the table (DMD_STM32 patches the same
        // entry for the FM6373)
        rw = gw = bw = (uint16_t)(((uint32_t)seq->scanReg << 8) | (rw & 0xC0) | ((numRows - 1) & 0x3F));
    }
    int slotSize = 16 * (m_numOutputSlots == 16 ? 12 : 6);
    uint8_t buf[192 * 2];
    int len = outputRegData(0, buf, rw, gw, bw, m_numOutputSlots);
    if (slotSize & 63) {
        // A slot that is not a whole number of 64 byte units shares a unit
        // with the one after it, so extend the write to cover that slot with
        // its own (constant) value rather than leave a partial unit: the
        // commit word for a five slot chip, the extra pre-commit word for a
        // six slot one.
        uint16_t next = seq->slots == 6 ? seq->extraWord : 0x0055;
        len = outputRegData(len, buf, next, next, next, m_numOutputSlots);
    }
    pru->memcpyToPRU((uint8_t*)&pruData->registers[0] + 2 * slotSize, buf, len);
}

void BBShiftPanelManager::writeDP3364SeqWord(int idx) {
    // rewrite the single rotating register slot with sequence word idx.  Same
    // in-flight rules as writeFM6373SeqWord: only safe while no register
    // upload is running.
    uint16_t rw = DP3364_SEQ_R[idx];
    uint16_t gw = DP3364_SEQ_G[idx];
    uint16_t bw = DP3364_SEQ_B[idx];
    if ((rw >> 8) == 0x02) {
        // scan row count
        rw = gw = bw = 0x0200 | ((numRows - 1) & 0x3F);
    } else if ((rw >> 8) == 0x08) {
        // linear output current multiplier.  The captured 0x7f is the value
        // the panel was designed around, so it is full brightness and this
        // only ever scales down from it; the per-color trim in 0x09/0x0b is
        // left alone so the white balance does not move with brightness.
        uint16_t cur = std::clamp(0x7F * m_brightness / 10, 8, 0x7F);
        rw = gw = bw = 0x0800 | cur;
    }
    int slotSize = 16 * (m_numOutputSlots == 16 ? 12 : 6);
    uint8_t buf[192 * 2];
    int len = outputRegData(0, buf, rw, gw, bw, m_numOutputSlots);
    if (slotSize & 63) {
        // A slot that is not a whole number of 64 byte units shares a unit
        // with the one after it, so pad with a second copy of the word to keep
        // the write on a unit boundary.  Nothing reads the padding - DP3364S
        // only sends one word per frame.
        len = outputRegData(len, buf, rw, gw, bw, m_numOutputSlots);
    }
    pru->memcpyToPRU((uint8_t*)&pruData->registers[0], buf, len);
}

void BBShiftPanelManager::setupGCLKConfig() {
    // Configuration for the GCLK program on the other PRU (see the rowConfig
    // register in BBShiftPanel_gclk.asm): [0] = blanking loops between GCLK
    // packets (the brightness knob), [1] = row select mode, [2]/[3] = GCLK
    // pulses in the first packet after a restart / every packet after
    pwmPru->data_ram[0] = m_brightness > 8 ? 3 : 11 - m_brightness;
    if (m_addressingMode == ADDRESSING_MODE_DP3364) {
        // DP3364S generates its own GCLK from an internal PLL, so this side
        // only emits the per-row ROW strobe (which replaces OE on this chip)
        // and advances the row driver.  Unlike the FM6373 the addressing
        // dropdown is honored: a panel with more than 32 scan rows cannot be
        // addressed by the 5 SEL lines at all and needs the token shift
        // register, which is what the row driver chips on these panels want.
        // Brightness comes from config register 0x08, not from blanking time.
        //   [2] = W4 ROW pulse width in 100ns units (W12 is emitted as 3x
        //         this; the chip only has to tell the two apart)
        //   [3] = row period in us
        int dclkNs = m_numOutputSlots == 16 ? DCLK_NS_16 : DCLK_NS_8;
        int minLine = std::max({ dp3364MinLineDCLKs(DP3364_SEQ_R, DP3364_SEQ_LEN),
                                 dp3364MinLineDCLKs(DP3364_SEQ_G, DP3364_SEQ_LEN),
                                 dp3364MinLineDCLKs(DP3364_SEQ_B, DP3364_SEQ_LEN) });
        pwmPru->data_ram[1] = 4 | (m_pwmDirectRow ? 1 : 0);
        // W4 is 4 DCLK periods; the pulse is emitted as a time because the
        // DCLK comes from the other PRU with no phase relationship to this one
        pwmPru->data_ram[2] = std::clamp((4 * dclkNs + 99) / 100, 2, 40);
        // The datasheet's line time is a minimum, and the row scan free runs,
        // so a row that is too short simply will not have finished displaying.
        // 2x the minimum is the margin for that; the scan still revisits every
        // row far faster than the eye at any supported scan count.
        pwmPru->data_ram[3] = std::clamp((2 * minLine * dclkNs + 999) / 1000, 4, 255);
        // [4] = PWM display group count, config register 0x03 + 1.  One W12
        // ROW pulse is emitted per (groups x rows) cycle, so this has to
        // track whatever DP3364_SEQ_* sets 0x03 to.
        pwmPru->data_ram[4] = (DP3364_SEQ_R[1] & 0x7F) + 1;
        return;
    }
    if (pwmChipSeqFor(m_addressingMode)) {
        // FM6373 family: single OE pulse per row, direct row select (the
        // only transport implemented for this family; the DP32019B boards
        // use it).  kingdo9 gives ICND1065L and SM16380SH the same OE style,
        // so they run this scan too.  Brightness comes from the chip's config
        // registers, not the blanking time.  [2] = opener pulse width us,
        // [3] = row period us (~128 DCLKs at the data clock rate, so the scan
        // rate is about the same while uploading and while free-running
        // between frames)
        pwmPru->data_ram[1] = 3;
        pwmPru->data_ram[2] = 2;
        pwmPru->data_ram[3] = 20;
        return;
    }
    pwmPru->data_ram[1] = m_pwmDirectRow ? 1 : 0;
    if (m_addressingMode == ADDRESSING_MODE_FM6353C ||
        m_addressingMode == ADDRESSING_MODE_ICND2153) {
        // DMD_STM32 GCLK_NUM: FM6353 row switching takes 138 GCLK pulses
        // (uniform; no first-packet extension is documented for this chip).
        // kingdo9's ICND2153 capture independently lands on 138 pulses per
        // scan, though it drives them as a half-rate waveform (one pulse per
        // two clock slots) that this GCLK program does not reproduce.
        pwmPru->data_ram[2] = 138;
        pwmPru->data_ram[3] = 138;
    } else {
        // FM6363: 74 pulses per row, first packet after a restart is 78
        // (logic analyzer capture; independently confirmed by the
        // kingdo9/rpi-rgb-led-matrix_pwm_experiment measurements)
        pwmPru->data_ram[2] = 78;
        pwmPru->data_ram[3] = 74;
    }
}

uint32_t BBShiftPanelManager::computeMaxBrightnessCycles() {
    // Scale the row length by the amount of data actually shifted per pixel
    // clock: 16 output slots shift twice the bytes of 8 slots, so the same
    // physical row takes twice as long to shift out
    uint32_t effLen = rowLen * m_numOutputSlots / 8;
    // Longer rows take longer to shift, so they get a longer maximum on-time
    // to keep the duty cycle (brightness) up at the cost of refresh rate.
    // This is a smooth ramp so that adding one panel cannot step the
    // brightness; it passes through 0x8800 at 256, 0xA800 at 384, 0xC800 at
    // 512 and 0xE800 at 640 (the values the old stepped defaults used).
    uint32_t maxBright = 64 * effLen + 18432;
    // The ramp and its bounds are tuned for 16 scan rows.  With fewer rows
    // each row owns a larger share of the frame so the on-times may grow
    // (1/8 scan); with more rows they must shrink or the refresh tanks
    // (1/32 scan P2.5 panels) - scale the bounds by 16/numRows so all scan
    // ratios get the same refresh contract.
    maxBright = std::clamp(maxBright, 0x8800u * 16 / numRows, 0xE800u * 16 / numRows);
    if (FileExists(FPP_DIR_MEDIA("/config/panel_timing.txt"))) {
        std::string v = GetFileContents(FPP_DIR_MEDIA("/config/panel_timing.txt"));
        if (!v.empty()) {
            maxBright = std::stoi(v, nullptr, 16);
        }
    }
    return maxBright;
}

void BBShiftPanelManager::buildStrideSchedule() {
    m_strideSchedule.clear();
    m_dupCopies.clear();

    uint32_t maxBright = computeMaxBrightnessCycles();
    // Approximate PRU cycles to shift one full stride out to the panels
    // (per-pixel cost including the amortized data load overhead).  In the
    // two-PRU configuration the OE PRU prefetches the ring blocks and hands
    // them over through the scratchpad, which shortens the load (measured:
    // 80.0 -> 83.2Hz on a 4-panel 12-bit chain, ~42 cycles/pixel effective)
    bool pruPrefetch = !singlePRU && !isPWMPanel();
    uint32_t cyclesPerPixel = (m_numOutputSlots == 16) ? (pruPrefetch ? 42 : 48)
                                                       : (pruPrefetch ? 25 : 26);
    uint32_t shiftCycles = rowLen * cyclesPerPixel + 100;

    // Capacity limit: the PRU brightness table holds 768 stride entries
    // (24 slots at 32 rows, so 1/32 scan panels keep a split-pulse budget)
    uint32_t bytesPerPixel = (m_numOutputSlots * 6 * 2) / 16;
    uint32_t strideLen = rowLen * bytesPerPixel;
    int maxSlots = 768 / (int)numRows;
    if (maxSlots < m_colorDepth) {
        // no room for splitting, the base schedule has to fit regardless
        maxSlots = m_colorDepth;
    }

    // Decide how many pulses each bit is displayed as.  A bit whose on-time
    // is >= k * shift time can be shown as k shorter pulses spread across the
    // frame at zero cost in frame time (the shifting still fits under each
    // pulse), which multiplies the perceived refresh rate.  The count rounds
    // to nearest: pieces slightly shorter than the shift time cost a little
    // dead time (bounded by half a shift per bit) but move that bit's light
    // to a much higher frequency - on longer chains the second MSB would
    // otherwise sit unsplit at the frame rate carrying a quarter of the light.
    int budget = maxSlots - m_colorDepth;
    std::vector<int> pieces(m_colorDepth, 1);
    for (int b = m_colorDepth - 1; b >= 0 && budget > 0; b--) {
        uint32_t on = (m_brightness * maxBright / 10) >> (m_colorDepth - 1 - b);
        int k = std::min((int)((on + shiftCycles / 2) / shiftCycles), 4);
        k = std::min(k, budget + 1);
        if (k <= 1) {
            // lower bits have shorter on-times, no more splitting is useful
            break;
        }
        pieces[b] = k;
        budget -= k - 1;
    }

    // Place the pieces on the timeline.  Each bit's pieces are spread evenly
    // across the frame with a half-spacing phase offset so that every pixel
    // value, not just full white, sees its light distributed across the whole
    // frame (e.g. 4 pieces land at 1/8, 3/8, 5/8, 7/8 and interleave with the
    // 2-piece bit at 1/4, 3/4).  Bits place MSB first; a collision moves to
    // the nearest free slot.
    int n = 0;
    for (int b = 0; b < m_colorDepth; b++) {
        n += pieces[b];
    }
    m_strideSchedule.assign(n, StrideSlot{ 0, false, 0 });
    std::vector<bool> used(n, false);
    for (int b = m_colorDepth - 1; b >= 0; b--) {
        uint32_t on = (m_brightness * maxBright / 10) >> (m_colorDepth - 1 - b);
        uint32_t per = on / pieces[b];
        for (int i = 0; i < pieces[b]; i++) {
            int target = (int)(((i + 0.5f) * n) / pieces[b]);
            int slot = target;
            for (int d = 0; d < n; d++) {
                // search outward from the target, alternating below/above
                int cand = (target + ((d & 1) ? (n - (d + 1) / 2) : (d / 2))) % n;
                if (!used[cand]) {
                    slot = cand;
                    break;
                }
            }
            used[slot] = true;
            // keep the total on-time exact, first piece takes the remainder
            uint32_t t = (i == 0) ? (on - per * (pieces[b] - 1)) : per;
            m_strideSchedule[slot] = { (uint8_t)b, false, t };
        }
    }
    bool seen[16] = { false };
    for (auto& slot : m_strideSchedule) {
        slot.primary = !seen[slot.bit];
        seen[slot.bit] = true;
    }

    // Frame buffer copies needed for the non-primary (duplicated) strides
    m_dupCopies.assign(numRows, {});
    int primarySlot[16];
    for (int s = 0; s < n; s++) {
        if (m_strideSchedule[s].primary) {
            primarySlot[m_strideSchedule[s].bit] = s;
        }
    }
    for (int s = 0; s < n; s++) {
        if (m_strideSchedule[s].primary) {
            continue;
        }
        int ps = primarySlot[m_strideSchedule[s].bit];
        for (int r = 0; r < numRows; r++) {
            uint32_t dstIdx = m_outputByRow ? (r * n + s) : (s * numRows + r);
            uint32_t srcIdx = m_outputByRow ? (r * n + ps) : (ps * numRows + r);
            m_dupCopies[r].emplace_back(dstIdx * strideLen, srcIdx * strideLen);
        }
    }

    // log what this schedule works out to
    uint64_t rowCycles = 0;
    uint64_t onCycles = 0;
    for (auto& slot : m_strideSchedule) {
        rowCycles += std::max(shiftCycles, slot.onTime) + 60;
        onCycles += slot.onTime;
    }
    uint64_t frameCycles = rowCycles * numRows;
    float refresh = 250000000.0f / (float)frameCycles;
    LogInfo(VB_CHANNELOUT, "BBShiftPanel: %d strides/row (%d bit color, MSB split %d ways), est refresh %d Hz, duty %d%%\n",
            n, m_colorDepth, pieces[m_colorDepth - 1], (int)refresh, (int)(onCycles * 100 / rowCycles));
    if (refresh < 60.0f) {
        m_refreshWarning = "LED panel refresh rate is only " + std::to_string((int)refresh) + "Hz and may flicker; reduce the color depth or panels per output";
        WarningHolder::AddWarning(m_refreshWarning);
    }
}

void BBShiftPanelManager::setupBrightnessValues() {
    uint32_t* cur = &pruData->brightness[0];
    int n = m_strideSchedule.size();
    auto writeEntry = [&](int s, int r) {
        int mappedRow = mapRow(r, m_addressingMode);
        cur[0] = m_strideSchedule[s].onTime;
        cur[1] = (mappedRow << 24) & 0x7F000000;
        if (m_outputByRow && m_outputBlankData && (s == 0)) {
            cur[1] |= 0x80000000;
        }
        // printf("Brightness[%d %d] = %08x  %08x\n", s, r, cur[0], cur[1]);
        cur += 2;
    };
    if (m_outputByRow) {
        for (int r = 0; r < numRows; r++) {
            for (int s = 0; s < n; s++) {
                writeEntry(s, r);
            }
        }
    } else {
        for (int s = 0; s < n; s++) {
            for (int r = 0; r < numRows; r++) {
                writeEntry(s, r);
            }
        }
    }
}

// Maps this matrix's channels onto the shared frame.  The lane a pixel lands
// in is fixed by which cape output its panel is on (outputPin/outputBank from
// the cape pinout), which is what lets two matrices share one frame without
// coordinating: they simply never compute the same destination offset.
bool BBShiftPanelOutput::buildScatterMap() {
    auto& mgr = BBShiftPanelManager::INSTANCE;
    const int m_panelWidth = mgr.m_panelWidth;
    const int m_panelHeight = mgr.m_panelHeight;
    const uint32_t rowLen = mgr.rowLen;
    const uint32_t maxRowLen = mgr.maxRowLen;
    const int m_numOutputSlots = mgr.m_numOutputSlots;

    PanelInterleaveHandler* handler = PanelInterleaveHandler::createHandler(mgr.m_panelInterleave, m_panelWidth, m_panelHeight, mgr.m_panelScan);
    if (!handler) {
        LogErr(VB_CHANNELOUT, "Failed to create panel interleave handler\n");
        return false;
    }

    uint32_t* channelOffsets = new uint32_t[m_channelCount];
    memset(channelOffsets, 0xFF, m_channelCount * sizeof(uint32_t));

    int pixelStride = m_numOutputSlots * 6;
    int totalRowLen = rowLen * pixelStride;
    for (int output = 0; output < mgr.m_numOutputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        // For 16 outputs, bank1 data starts at offset 48 (= 8 slots * 6 colors)
        // so that ISPC's 16-lane reduce_add packs bank0/bank1 pairs correctly:
        // bank0 channels occupy slots 0-47, bank1 channels occupy slots 48-95
        int outputIdx = mgr.outputPin[output] + mgr.outputBank[output] * 48;

        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int c = m_panelMatrix->m_panels[panel].chain;
            // the chain position is measured from the far end of the LONGEST
            // chain on the cape, so a matrix with a shorter chain than another
            // simply shifts the leading pixels off the end of its own chain
            int chain = mgr.m_longestChain - c - 1;
            int xOff = chain * maxRowLen;

            if (mgr.m_dataLayout) {
                // Full height layout: lane 1 and lane 2 carry the two halves
                // of the SAME row.  Layout 2 puts the left half on lane 1,
                // layout 1 swaps them.  Every row is its own scan address, so
                // yOut is just y and the row is half a panel wide.
                const int half = m_panelWidth / 2;
                const int leftLane1 = (mgr.m_dataLayout == 2);
                for (int y = 0; y < m_panelHeight; y++) {
                    int yw = y * m_panelWidth * 3;
                    for (int x = 0; x < half; ++x) {
                        int lane1x = leftLane1 ? x : x + half;
                        int lane2x = leftLane1 ? x + half : x;
                        uint32_t r1 = m_panelMatrix->m_panels[panel].pixelMap[yw + lane1x * 3];
                        uint32_t g1 = m_panelMatrix->m_panels[panel].pixelMap[yw + lane1x * 3 + 1];
                        uint32_t b1 = m_panelMatrix->m_panels[panel].pixelMap[yw + lane1x * 3 + 2];

                        uint32_t r2 = m_panelMatrix->m_panels[panel].pixelMap[yw + lane2x * 3];
                        uint32_t g2 = m_panelMatrix->m_panels[panel].pixelMap[yw + lane2x * 3 + 1];
                        uint32_t b2 = m_panelMatrix->m_panels[panel].pixelMap[yw + lane2x * 3 + 2];

                        int yOut = y;
                        int xOut = x + xOff;
                        if (mgr.isPWMPanel()) {
                            int xo2 = xOut % 16;
                            int xo3 = xOut / 16;
                            xOut = xo2 * (rowLen / 16) + xo3;
                        }
                        channelOffsets[r1] = yOut * totalRowLen + xOut * pixelStride + outputIdx;
                        channelOffsets[g1] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 8;
                        channelOffsets[b1] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 16;
                        channelOffsets[r2] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 24;
                        channelOffsets[g2] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 32;
                        channelOffsets[b2] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 40;
                    }
                }
                continue;
            }

            for (int y = 0; y < (m_panelHeight / 2); y++) {
                int yw1 = y * m_panelWidth * 3;
                int yw2 = (y + (m_panelHeight / 2)) * m_panelWidth * 3;

                for (int x = 0; x < m_panelWidth; ++x) {
                    uint32_t r1 = m_panelMatrix->m_panels[panel].pixelMap[yw1 + x * 3];
                    uint32_t g1 = m_panelMatrix->m_panels[panel].pixelMap[yw1 + x * 3 + 1];
                    uint32_t b1 = m_panelMatrix->m_panels[panel].pixelMap[yw1 + x * 3 + 2];

                    uint32_t r2 = m_panelMatrix->m_panels[panel].pixelMap[yw2 + x * 3];
                    uint32_t g2 = m_panelMatrix->m_panels[panel].pixelMap[yw2 + x * 3 + 1];
                    uint32_t b2 = m_panelMatrix->m_panels[panel].pixelMap[yw2 + x * 3 + 2];
                    int yOut = y;
                    int xOut = x;
                    handler->map(xOut, yOut);
                    xOut += xOff;

                    if (mgr.isPWMPanel()) {
                        // For PWM panels, the first of each group of 16 pixels is out first,
                        // then the second of each group of 16, etc...
                        int xo2 = xOut % 16;
                        int xo3 = xOut / 16;

                        xOut = xo2 * (rowLen / 16) + xo3;
                    }
                    // Color stride is always 8: within each bank, 8 slots per color channel.
                    // For 8 outputs: outputIdx=pin(0-7), channels at +0,+8,+16,+24,+32,+40
                    // For 16 outputs bank0: same; bank1: outputIdx=pin+48, same offsets
                    channelOffsets[r1] = yOut * totalRowLen + xOut * pixelStride + outputIdx;
                    channelOffsets[g1] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 8;
                    channelOffsets[b1] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 16;
                    channelOffsets[r2] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 24;
                    channelOffsets[g2] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 32;
                    channelOffsets[b2] = yOut * totalRowLen + xOut * pixelStride + outputIdx + 40;
                }
            }
        }
    }
    delete handler;

    // Build the scatter map sorted by destination offset so the prep loop
    // writes sequentially; the random accesses become byte reads, which the
    // cache handles far better than random read-modify-write stores.
    // Channels not covered by any panel (partially invalid layouts, unused
    // canvas regions) still have the 0xFFFFFFFF fill from above and are left
    // out of the map; writing through the marker used to crash.  The stable
    // sort keeps the original channel order for duplicated offsets so the
    // last writer still wins.
    std::vector<std::pair<uint32_t, uint32_t>> map;
    map.reserve(m_channelCount);
    uint32_t unmapped = 0;
    for (int x = 0; x < m_channelCount; x++) {
        if (channelOffsets[x] != 0xFFFFFFFF) {
            map.emplace_back(channelOffsets[x], x);
        } else {
            ++unmapped;
        }
    }
    if (unmapped) {
        LogWarn(VB_CHANNELOUT, "BBShiftPanel: %u of %u channels are not mapped to any panel; check the panel layout\n",
                unmapped, m_channelCount);
    }
    std::stable_sort(map.begin(), map.end(),
                     [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) { return a.first < b.first; });
    m_scatterOffsets.resize(map.size());
    m_scatterSrc.resize(map.size());
    for (size_t x = 0; x < map.size(); x++) {
        m_scatterOffsets[x] = map[x].first;
        m_scatterSrc[x] = map[x].second;
    }
    delete[] channelOffsets;
    return true;
}

void BBShiftPanelOutput::setupGamma() {
    auto& mgr = BBShiftPanelManager::INSTANCE;
    // The OE on-time is shared by every matrix on the cape, so a matrix that
    // asked for less brightness than the cape ended up running at makes up the
    // difference here.  With one matrix, or with matrices that agree, the
    // ratio is 1.0 and the table is unchanged.
    float brightnessScale = 1.0f;
    if (mgr.m_brightness > 0 && m_brightness < mgr.m_brightness) {
        brightnessScale = (float)m_brightness / (float)mgr.m_brightness;
    }

    int colorDepth = mgr.m_colorDepth;
    if (mgr.isPWMPanel()) {
        // we are outputting 16 bit data as that's what the
        // PWM registers require, but only the bottom 12 bits are used
        colorDepth = 12;
    }

    GammaLUT::BuildForColorDepth(gammaCurve, GammaLUT::Clamp(m_gamma, 2.2f), colorDepth, brightnessScale);
    /*
    for (int x = 0; x < 256; x++) {
        printf("%d: %04x\n", x, gammaCurve[x]);
    }
    */
}

void BBShiftPanelOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    for (int output = 0; output < BBShiftPanelManager::INSTANCE.numOutputs(); output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];

            m_panelMatrix->m_panels[panel].drawTestPattern(channelData + m_startChannel, cycleNum, percentOfCycle, testType);
        }
    }
}

void BBShiftPanelOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

void BBShiftPanelOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "BBShiftPanelOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    Width          : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    Height         : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "    Longest Chain  : %d\n", m_longestChain);
    LogDebug(VB_CHANNELOUT, "    Inverted Data  : %d\n", m_invertedData);
    LogDebug(VB_CHANNELOUT, "    Brightness     : %d\n", m_brightness);
    LogDebug(VB_CHANNELOUT, "    Gamma          : %f\n", m_gamma);
    BBShiftPanelManager::INSTANCE.dumpConfig();

    ChannelOutput::DumpConfig();
}

void BBShiftPanelManager::dumpConfig() {
    LogDebug(VB_CHANNELOUT, "  Shared cape config (%d matri%s):\n",
             (int)m_members.size(), m_members.size() == 1 ? "x" : "ces");
    LogDebug(VB_CHANNELOUT, "    Panel Width    : %d\n", m_panelWidth);
    LogDebug(VB_CHANNELOUT, "    Panel Height   : %d\n", m_panelHeight);
    LogDebug(VB_CHANNELOUT, "    Color Depth    : %d\n", m_colorDepth);
    LogDebug(VB_CHANNELOUT, "    Longest Chain  : %d\n", m_longestChain);
    LogDebug(VB_CHANNELOUT, "    Cape Brightness: %d\n", m_brightness);
    LogDebug(VB_CHANNELOUT, "    Output Rows    : %d\n", numRows);
    LogDebug(VB_CHANNELOUT, "    Output Length  : %d\n", rowLen);
    LogDebug(VB_CHANNELOUT, "    Num Outputs    : %d (%d slots)\n", m_numOutputs, m_numOutputSlots);
    LogDebug(VB_CHANNELOUT, "    Addressing Mode: %d %s\n", m_addressingMode, isPWMPanel() ? "PWM" : "Shift");
    if (!m_strideSchedule.empty()) {
        std::string sched;
        for (auto& slot : m_strideSchedule) {
            sched += " " + std::to_string(slot.bit) + (slot.primary ? "" : "*") + "/" + std::to_string(slot.onTime);
        }
        LogDebug(VB_CHANNELOUT, "    Stride Schedule:%s\n", sched.c_str());
    }
}
