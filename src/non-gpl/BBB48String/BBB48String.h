#pragma once
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

#include <string>
#include <vector>

#include "channeloutput/ChannelOutput.h"
#include "channeloutput/PixelString.h"
#include "util/BBBPruUtils.h"

#define MAX_WS2811_TIMINGS 128

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;

    // write data length to start, 0xFFFF to abort
    volatile uint32_t command;
    volatile uint32_t response;

    uint16_t timings[MAX_WS2811_TIMINGS];
} __attribute__((__packed__)) BBB48StringData;

class BBB48StringOutput : public ChannelOutput {
public:
    BBB48StringOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBB48StringOutput();

    virtual std::string GetOutputType() const {
        return "BBB Pixel Strings";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;
    virtual void PrepData(unsigned char* channelData) override;
    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType) override;
    virtual bool SupportsTesting() const { return  true; }

private:
    void StopPRU(bool wait = true);
    int StartPRU(bool both);

    std::string m_subType;

    class FrameData {
    public:
        std::vector<int> gpioStringMap;
        uint8_t* lastData = nullptr;
        uint8_t* curData = nullptr;
        uint32_t frameSize = 0;
        int maxStringLen = 0;
        bool copyToPru = true;
        uint32_t outputStringLen;
    } m_gpio0Data, m_gpioData;
    std::vector<PixelString*> m_strings;

    uint32_t m_curFrame;
    int m_stallCount;
    uint32_t m_licensedOutputs;

    BBBPru* m_pru;
    BBB48StringData* m_pruData;

    BBBPru* m_pru0;
    BBB48StringData* m_pru0Data;

    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;

    void prepData(FrameData& d, unsigned char* channelData);
    void sendData(FrameData& d, uintptr_t* dptr);
    void createOutputLengths(FrameData& d, const std::string& pfx, std::vector<std::string>& args);
};
