#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2022 by the Falcon Player Developers.
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

constexpr int NUM_STRINGS_PER_PIN = 8;
constexpr int MAX_PINS_PER_PRU = 8;

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;

    // write data length to start, 0xFFFF to abort. will be cleared when started
    volatile uint32_t command;
    volatile uint32_t response;

    uint32_t buffer[3]; // need a bit of a buffer
    uint32_t commandTable[1789];
} __attribute__((__packed__)) BBShiftStringData;

class BBShiftStringOutput : public ChannelOutput {
public:
    BBShiftStringOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBShiftStringOutput();

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
    virtual bool SupportsTesting() const { return true; }

private:
    void StopPRU(bool wait = true);
    int StartPRU();

    std::string m_subType;

    class FrameData {
    public:
        FrameData() {
            for (int y = 0; y < MAX_PINS_PER_PRU; ++y) {
                for (int x = 0; x < NUM_STRINGS_PER_PIN; ++x) {
                    stringMap[y][x] = -1;
                }
            }
        }
        ~FrameData() {
            if (channelData)
                free(channelData);
            if (formattedData)
                free(formattedData);
        }
        std::array<std::array<int, NUM_STRINGS_PER_PIN>, MAX_PINS_PER_PRU> stringMap;
        BBBPru* pru = nullptr;
        BBShiftStringData* pruData = nullptr;

        uint8_t* channelData = nullptr;
        uint8_t* formattedData = nullptr;

        uint8_t* lastData = nullptr;
        uint8_t* curData = nullptr;
        uint32_t frameSize = 0;
        int maxStringLen = 0;
    } m_pru0, m_pru1;

    std::vector<PixelString*> m_strings;

    uint32_t m_curFrame = 0;
    uint32_t m_licensedOutputs = 0;

    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;

    void prepData(FrameData& d, unsigned char* channelData);
    void sendData(FrameData& d);

    void createOutputLengths(FrameData& d, const std::string& pfx);
};
