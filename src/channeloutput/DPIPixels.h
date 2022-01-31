#pragma once
/*
 *   Raspberry Pi DPI Pixels handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2022 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <vector>

// from fbws.c
#include <linux/fb.h>

#include "PixelString.h"
#include "ThreadedChannelOutputBase.h"
#include "util/SPIUtils.h"

class DPIPixelsOutput : public ThreadedChannelOutputBase {
public:
    DPIPixelsOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~DPIPixelsOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    bool FrameBufferIsConfigured(void);

    void InitFrameWS281x(void);
    void OutputPixelRowWS281x(uint32_t *rowData);

    std::string device;
    std::string protocol;

    std::vector<PixelString*> m_strings;
    int bitPos[24];

    int fbfd = 0;
    char *fbp = 0;
    int screensize = 0;
    int pagesize = 0;
    int page = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    int longestString = 0;

    int protoBitsPerLine = 0;
    int protoLastBitPerLine = 0;
    int protoBitOnLine = 0;
    char *protoDest = nullptr;
};
