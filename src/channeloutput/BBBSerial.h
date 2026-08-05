#pragma once
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

#include <cstddef>
#include <cstdint>
#include <string>
#include "fpp-json-fwd.h"
#include <vector>

using namespace ::std;

#include "ThreadedChannelOutput.h"
#include "util/BBBPruUtils.h"

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU.  This is a PRU-side physical address,
    // so it is 32 bit wide no matter how wide a pointer is on the ARM side -
    // FalconSerial.asm reads it with a fixed "LBCO &r0, CONST_PRUDRAM, 0, 8"
    // that expects the command in the very next word.
    volatile uint32_t address_dma;

    // write 1 to start, 0xFF to abort. will be cleared when started
    volatile unsigned command;
    volatile unsigned response;
} __attribute__((__packed__)) BBBSerialData;

// FalconSerial.asm hard-codes these offsets (LBCO/SBCO against CONST_PRUDRAM),
// so the ARM-side view has to match on both 32 and 64 bit builds.
static_assert(offsetof(BBBSerialData, address_dma) == 0, "PRU expects address_dma at offset 0");
static_assert(offsetof(BBBSerialData, command) == 4, "PRU expects command at offset 4");
static_assert(offsetof(BBBSerialData, response) == 8, "PRU expects response at offset 8");

class BBBSerialOutput : public ThreadedChannelOutput {
public:
    BBBSerialOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBBSerialOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    int m_outputs;
    int m_pixelnet;
    vector<int> m_startChannels;

    uint8_t* m_lastData;
    uint8_t* m_curData;
    uint32_t m_curFrame;

    BBBPru* m_pru;
    uint8_t* m_ddrArea = nullptr;
    uint32_t m_ddrPhys = 0;
    BBBSerialData* m_serialData;
    bool m_pruStalled = false;
};
