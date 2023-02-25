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
#include "FrameBuffer.h"
#include "util/SPIUtils.h"

constexpr int MAX_DPI_PIXEL_BANKS = 3;

class DPIPixelsOutput : public ChannelOutput {
public:
    DPIPixelsOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~DPIPixelsOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    int GetDPIPinBitPosition(std::string pinName);

    bool FrameBufferIsConfigured(void);

    bool InitializeWS281x(void);
    void InitFrameWS281x(void);
    void OutputPixelRowWS281x(uint32_t* rowData, int maxString);
    void CompleteFrameWS281x(void);

    std::string device = "fb1";
    std::string protocol = "ws2811";
    int licensedOutputs = 0;
    bool usingSmartReceivers = false;
    bool usingLatches = false;
    uint8_t* onOffMap = nullptr;

    std::vector<PixelString*> pixelStrings;
    int bitPos[24];

    FrameBuffer *fb = nullptr;
    int fbPage = -1;

    int stringCount = 0;
    int longestString = 0;
    int longestStringInBank[MAX_DPI_PIXEL_BANKS];
    uint32_t latchPinMask = 0x000000;
    uint32_t latchPinMasks[MAX_DPI_PIXEL_BANKS];

    int protoBitsPerLine = 0;
    int protoBitOnLine = 0;
    uint8_t* protoDest = nullptr;
    int protoDestExtra = 0;

    // WS281x vars
    int fbPixelMult = 1;
};
