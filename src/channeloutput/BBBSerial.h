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

#include <string>
#include <vector>

using namespace ::std;

#include "ThreadedChannelOutput.h"
#include "util/BBBPruUtils.h"

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;

    // write 1 to start, 0xFF to abort. will be cleared when started
    volatile unsigned command;
    volatile unsigned response;
} __attribute__((__packed__)) BBBSerialData;

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
    BBBSerialData* m_serialData;
};
