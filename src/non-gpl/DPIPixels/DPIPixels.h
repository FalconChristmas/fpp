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
#include "framebuffer/FrameBuffer.h"
#include "util/SPIUtils.h"

constexpr int MAX_DPI_PIXEL_BANKS = 3;

class DPIPixelsOutput : public ChannelOutput {
public:
    DPIPixelsOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~DPIPixelsOutput();

    virtual std::string GetOutputType() const {
        return "DPI Pixels";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const override { return true; }

private:
    int GetDPIPinBitPosition(std::string pinName);
    bool FrameBufferIsConfigured(void);

    bool InitializeWS281x(void);
    void InitFrameWS281x(void);
    void OutputPixelRowWS281x(uint8_t* rowData, int maxString);
    void CompleteFrameWS281x(void);

    std::string device = "fb1";
    std::string protocol = "ws2811";
    int licensedOutputs = 0;
    bool usingSmartReceivers = false;
    bool usingLatches = false;
    uint8_t* onOffMap = nullptr;

    std::vector<PixelString*> pixelStrings;
    int bitPos[24];

    FrameBuffer* fb = nullptr;
    int fbPage = -1;
    bool displayEnabled = false;  // Track if CRTC/plane is enabled
    bool initComplete = false;    // Track if initialization is complete
    int nonZeroFrameCount = 0;    // Track consecutive frames with significant data

    int stringCount = 0;
    int longestString = 0;
    int longestStringInBank[MAX_DPI_PIXEL_BANKS];
    int firstStringInBank[MAX_DPI_PIXEL_BANKS];
    uint32_t latchPinMask = 0x000000;
    uint32_t latchPinMasks[MAX_DPI_PIXEL_BANKS];
    uint32_t nonLatchPins = 0xFFFFFF;  // All pins except latches

    int protoBitsPerLine = 0;
    int protoBitOnLine = 0;
    uint8_t* protoDest = nullptr;
    int protoDestExtra = 0;

    // WS281x vars
    int fbPixelMult = 1;
    int fbEndBufferSize = 1;
    int ws281xResetLines = 10;  // Number of blank lines at top of FB for WS281x RESET
    
    // Track which pins we configured as DPI so we can reset them on close
    std::vector<std::string> m_configuredDpiPins;

    // output testing data
    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;

    std::list<std::string> m_autoCreatedModelNames;
};
