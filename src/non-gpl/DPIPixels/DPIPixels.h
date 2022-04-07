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

#include <linux/fb.h>

#include "ChannelOutputBase.h"
#include "PixelString.h"
#include "util/SPIUtils.h"

class DPIPixelsOutput : public ChannelOutputBase {
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

    std::string device = "/dev/fb1";
    std::string protocol = "ws2811";

    std::vector<PixelString*> pixelStrings;
    int bitPos[24];

    int fbfd = 0;
    char* fbp = 0;
    int screensize = 0;
    int pagesize = 0;
    int page = 1;
    int pages = 3;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    int stringCount = 0;
    int longestString = 0;

    int protoBitsPerLine = 0;
    int protoBitOnLine = 0;
    uint8_t* protoDest = nullptr;
    int protoDestExtra = 0;
};

