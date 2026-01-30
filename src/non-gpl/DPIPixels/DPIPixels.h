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

constexpr int MAX_DPI_PIXEL_LATCHES = 4;

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
    void WriteDataAtPosition(uint8_t *&ptr, const uint32_t value);
    void WriteLatchedDataAtPosition(uint8_t *&ptr, const uint32_t value, const uint32_t latches);

    bool InitializeWS281x(void);

    std::string device = "DPI-1";
    std::string protocol = "ws2811";
    int licensedOutputs = 0;
    bool usingSmartReceivers = false;
    bool usingLatches = false;
    uint8_t* onOffMap = nullptr;

    std::vector<PixelString*> pixelStrings;
    int bitPos[MAX_DPI_PIXEL_LATCHES * 24];

    FrameBuffer* fb = nullptr;
    int fbPage = -1;

    int latchCount = 0;
    int longestString = 0;
    int outputToStringMap[MAX_DPI_PIXEL_LATCHES * 24];
    int stringToOutputMap[MAX_DPI_PIXEL_LATCHES * 24];
    int stringLengths[MAX_DPI_PIXEL_LATCHES * 24];
    uint32_t latchPinMask = 0x000000;
    uint32_t latchPinMasks[MAX_DPI_PIXEL_LATCHES];
    uint32_t onOffMask[MAX_DPI_PIXEL_LATCHES][4800]; // Max 1600 pixels * 3 channels each.

    int protoBitsPerLine = 0;
    int protoBitOnLine = 0;
    uint8_t* protoDest = nullptr;
    int protoDestExtra = 0;

    int fbPixelMult = 1;

    // output testing data
    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;

    std::list<std::string> m_autoCreatedModelNames;
};


// This function writes out 16 FB pixels at a time as 12 uint32_t.
// Used by non-latch mode since we need to write out the same data 16 times.
inline void DPIPixelsOutput::WriteDataAtPosition(uint8_t *&ptr, const uint32_t value) {
    // value is 24-bit value in a uint32_t
    // FB uint32_t   value       new uint32_t  fb0 order
    // v1            0x00abcdef  0xefabcdef    ef cd ab ef
    // v2            0x00abcdef  0xcdefabcd    cd ab ef cd
    // v3            0x00abcdef  0xabcdefab    ab ef cd ab

    uint32_t v1 = value | (value << 24);
    uint32_t v2 = (value << 16) | (value >> 8);
    uint32_t v3 = (value << 8) | (value >> 16);
    uint32_t *tptr = (uint32_t*)ptr;

    // Write out first 4 FB pixels
    *(tptr++) = v1;
    *(tptr++) = v2;
    *(tptr++) = v3;

    // Copy data from first four to next 8
    memcpy(ptr + 12, ptr, 12);
    memcpy(ptr + 24, ptr, 24);

    ptr += 48; // 12 uint32_t
}

// This function writes out 4 FB pixels at a time as 3 uint32_t.
// Used by Latch mode since we write out data for each latch 4 FB pixels at a time.
inline void DPIPixelsOutput::WriteLatchedDataAtPosition(uint8_t *&ptr, const uint32_t value, const uint32_t latches) {
    // value and valueWithLatch are each 24-bit values in a uint32_t
    // FB uint32_t   value       valueWithLatch  new uint32_t  fb0 order
    // #1            0x00abcdef  0x00ghijkl      0xklabcdef    ef cd ab kl
    // #2            0x00abcdef  0x00ghijkl      0xijklghij    ij gh kl ij
    // #3            0x00abcdef  0x00ghijkl      0xabcdefgh    gh ef cd ab

    uint32_t valueWithLatch = value | latches;
    uint32_t *tptr = (uint32_t*)ptr;

    *(tptr++) = value | (valueWithLatch << 24);
    *(tptr++) = (valueWithLatch << 16) | (valueWithLatch >> 8);
    *(tptr++) = (value << 8) | (valueWithLatch >> 16);

    ptr += 12;
}

