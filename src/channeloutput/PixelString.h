#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <cmath>
#include <stdint.h>
#include <string>
#include <vector>

#include "ColorOrder.h"

class VirtualString {
public:
    VirtualString();
    VirtualString(int receiverNum);
    ~VirtualString(){};

    int channelsPerNode() const;

    int startChannel;
    int pixelCount;
    int groupCount;
    int reverse;
    FPPColorOrder colorOrder;
    int startNulls;
    int endNulls;
    int zigZag;
    int brightness;
    float gamma;
    uint8_t brightnessMap[256];

    int whiteOffset;
    int8_t receiverNum;
    uint8_t leadInCount;
    uint8_t toggleCount;
    uint8_t leadOutCount;
    std::string description;

    int *chMap;
    int chMapCount;
};

class GPIOCommand {
public:
    GPIOCommand(int p, int offset, int tp = 0, int bo = 0) :
        port(p),
        channelOffset(offset),
        type(tp),
        bitOffset(bo) {}
    ~GPIOCommand() {}

    int port;
    int channelOffset;
    int type; //0 for off, 1 for on
    int bitOffset;
};

class PixelString {
public:
    PixelString(bool supportsSmartReceivers = false);
    ~PixelString();

    int Init(Json::Value config, Json::Value *pinConfig = nullptr);
    void DumpConfig(void);

    int m_portNumber;
    int m_channelOffset;
    int m_outputChannels;
    uint8_t *m_outputBuffer;

    std::vector<VirtualString> m_virtualStrings;
    std::vector<GPIOCommand> m_gpioCommands;

    std::vector<int> m_outputMap;
    uint8_t** m_brightnessMaps;

    bool m_isSmartReceiver;

    static void AutoCreateOverlayModels(const std::vector<PixelString*>& strings);

    // returned buffer is owned by the PixelString and reused next frame
    uint8_t *prepareOutput(uint8_t *channelData);

private:
    void SetupMap(int vsOffset, const VirtualString& vs);
    void FlipPixels(int offset1, int offset2, int chanCount);
    void DumpMap(const char* msg);

    int ReadVirtualString(Json::Value& vsc, VirtualString& vs) const;
    void AddVirtualString(const VirtualString& vs);
    void AddNullPixelString();
};
