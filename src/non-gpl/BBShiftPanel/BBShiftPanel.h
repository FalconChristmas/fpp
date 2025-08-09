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

#include <queue>
#include <string>
#include <vector>

#include "channeloutput/ChannelOutput.h"
#include "util/BBBPruUtils.h"

#include "../../channeloutput/Matrix.h"
#include "../../channeloutput/PanelMatrix.h"

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uint32_t address_dma;

    union {
        struct {
            volatile uint16_t command;
            volatile uint16_t result;
        } __attribute__((__packed__));
        struct {
            // Standard shift register based panels where the data is shifted out row by row and the PWM
            // is handled by the PRU code via the OE pin
            // write data length to start, 0xFFFF to abort. will be cleared when started
            uint16_t pixelsPerStride;
            uint16_t numStrides;

            // 2 uint32_t for each stride
            // first uint32_t is the brightness, number of clock ticks for on
            // second uint32_t - bit 32 is flag to output black after this row,  bits 25-31 is the address, lower 24 is extra "off" time
            uint32_t brightness[12 * 32 * 2]; // 12bits*32 rows * 2
        } __attribute__((__packed__));
        struct {
            // Panels that handle the PWM themselves, all data is shifted out and the panel displays it automatically
            uint16_t cmd;
            uint8_t numBlocks;
            uint8_t numRows;
            uint16_t buffer[4];            // buffer to get registers aligned on boundary
            uint8_t registers[5 * 6 * 16]; // 5 registers for each of r/g/b/r2/b2/g2, 16 bits each
        } __attribute__((__packed__));
    } __attribute__((__packed__));
} __attribute__((__packed__)) BBShiftPanelData;

class BBShiftPanelOutput : public ChannelOutput {
public:
    BBShiftPanelOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBShiftPanelOutput();

    virtual std::string GetOutputType() const {
        return "BB64 Panels";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;
    virtual void PrepData(unsigned char* channelData) override;
    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const { return true; }

private:
    void PrepDataShift();
    void PrepDataPWM();

    void StopPRU(bool wait = true);
    int StartPRU();

    bool isPWMPanel();

    void setupGamma(float gamma);
    bool setupChannelOffsets();

    void setupBrightnessValues();
    void setupPWMRegisters();

    BBBPru* pru = nullptr;
    BBBPru* pwmPru = nullptr;
    BBShiftPanelData* pruData = nullptr;

    Matrix* m_matrix = nullptr;
    PanelMatrix* m_panelMatrix = nullptr;

    int m_panelWidth = 0;
    int m_panelHeight = 0;
    int m_panelScan = 0;
    std::string m_panelInterleave = "";

    int m_addressingMode = 0;
    int m_longestChain = 0;
    int m_invertedData = 0;
    int m_brightness = 10;
    int m_colorDepth = 12;

    FPPColorOrder m_colorOrder;

    int m_panels = 0;
    int m_width = 0;
    int m_height = 0;

    bool m_outputByRow = false;
    bool m_outputBlankData = false;
    int addressingType = 0;

    uint16_t gammaCurve[256];
    uint8_t outputMap[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    uint32_t* channelOffsets = nullptr;
    uint16_t* currentChannelData = nullptr;

    // Four output buffers allows for double buffering and extras for the PRU to read from while the next frame is being built
    // With 4MB of memory reserved for transfer to PRU, 12 P5 panels per output with 12bit color depth (total 3.5MB)
    constexpr static int NUM_OUTPUT_BUFFERS = 4;
    std::array<uint8_t*, NUM_OUTPUT_BUFFERS> outputBuffers = { nullptr, nullptr, nullptr, nullptr };
    uint8_t currOutputBuffer = 0;
    uint32_t numRows = 0;
    uint32_t rowLen = 0;

    bool singlePRU = false;
    std::string m_autoCreatedModelName;

    std::queue<std::function<void()>> bgTasks;
    volatile bool bgThreadsRunning = false;
    std::mutex bgTaskMutex;
    std::condition_variable bgTaskCondVar;
    std::atomic<int> bgThreadCount;
    void runBackgroundTasks();
    void processTasks(std::atomic<int>& counter);
};
