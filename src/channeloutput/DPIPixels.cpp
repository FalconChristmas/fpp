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
#include "fpp-pch.h"

#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "DPIPixels.h"
#include "util/GPIOUtils.h"

#define _0H 1
#define _0L 2
#define _1H 2
#define _1L 1
#define _H(b) ((b) ? _1H : _0H)
#define _L(b) ((b) ? _1L : _0L)

#define POSITION_TO_BITMASK(x) (0x800000 >> x)

/*
 * https://www.raspberrypi.com/documentation/computers/raspberry-pi.html
 * https://www.raspberrypi.com/documentation/computers/config_txt.html
 *
 * To use this code, the framebuffer must be setup to a specific resolution
 * and timing.  This is normally handled by setupChannelOutputs in
 * /opt/fpp/scripts/functions.  Below are sample configs for /boot/config.txt
 *
 * # DPI Pixels FB config.txt entries:
 * enable_dpi_lcd=1
 * dpi_group=2
 * dpi_mode=87
 * dpi_output_format=0x17
 * max_framebuffers=2
 *
 * NOTE: A 'dpi_timings' line is also needed to set the refresh rate and max
 * pixels per string.  These are currently only documented in the functions
 * script in case they are changed.  The auto-detect in FrameBufferIsConfigured()
 * will also need to be updated in this case to detect the new resolution(s).
 *
 */

extern "C" {
DPIPixelsOutput* createOutputDPIPixels(unsigned int startChannel,
                                       unsigned int channelCount) {
    return new DPIPixelsOutput(startChannel, channelCount);
}
}

/////////////////////////////////////////////////////////////////////////////

DPIPixelsOutput::DPIPixelsOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutputBase(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::DPIPixelsOutput(%u, %u)\n",
             startChannel, channelCount);
}

DPIPixelsOutput::~DPIPixelsOutput() {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::~DPIPixelsOutput()\n");

    for (int s = 0; s < stringCount; s++)
        delete pixelStrings[s];
}

int DPIPixelsOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Init(JSON)\n");

    std::string subType = config["subType"].asString();
    if (subType == "") {
        subType = "DPIPixels";
    }
    Json::Value root;
    char filename[256];

    std::string verPostf = "";
    sprintf(filename, "/home/fpp/media/tmp/strings/%s%s.json", subType.c_str(), verPostf.c_str());
    if (!FileExists(filename)) {
        sprintf(filename, "/opt/fpp/capes/pi/strings/%s%s.json", subType.c_str(), verPostf.c_str());
    }
    if (!FileExists(filename)) {
        LogErr(VB_CHANNELOUT, "No output pin configuration for %s%s\n", subType.c_str(), verPostf.c_str());
        return 0;
    }
    if (!LoadJsonFromFile(filename, root)) {
        LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s%s\n", subType.c_str(), verPostf.c_str());
        return 0;
    }

    // Setup bit offsets for 24 outputs
    for (int i = 0; i < 24; i++) {
        bitPos[i] = -1; // unused pin, no output assigned
    }

    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString();

        if (!newString->Init(s))
            return 0;

        if ((newString->m_outputChannels / 3) > longestString)
            longestString = newString->m_outputChannels / 3;

        if (protocol == "")
            protocol = s["protocol"].asString();

        if (root["outputs"][i].isMember("pin")) {
            std::string pinName = root["outputs"][i]["pin"].asString();

            if (pinName[0] == 'P') {
                const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
                pin.configPin("dpi");

                bitPos[i] = GetDPIPinBitPosition(pinName);
            }
        }

        pixelStrings.push_back(newString);
    }

    stringCount = pixelStrings.size();
    LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", stringCount);

    // Open the Frame Buffer for reading and writing
    if (FileExists("/dev/fb1")) {
        fbfd = open("/dev/fb1", O_RDWR);
        if (!fbfd || ((int)fbfd == -1)) {
            LogDebug(VB_CHANNELOUT, "Error: cannot open framebuffer device.\n");
            return 0;
        }
    }

    if (!FrameBufferIsConfigured() && FileExists("/dev/fb0")) {
        // Try /dev/fb0 if /dev/fb1 isn't configured
        close(fbfd);

        LogDebug(VB_CHANNELOUT, "/dev/fb1 is not configured for DPI Pixels, trying /dev/fb0\n");

        device = "/dev/fb0";
        fbfd = open("/dev/fb0", O_RDWR);
        if (!fbfd || ((int)fbfd == -1)) {
            LogDebug(VB_CHANNELOUT, "Error: /dev/fb1 is not configured and can not open /dev/fb0\n");
            return 0;
        }

        if (!FrameBufferIsConfigured()) {
            LogErr(VB_CHANNELOUT, "Error: Neither /dev/fb0 or /dev/fb1 are configured for DPI Pixels.\n");
            return 0;
        }
    }

    LogDebug(VB_CHANNELOUT, "The framebuffer device %s was opened successfully.\n", device.c_str());

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
        LogDebug(VB_CHANNELOUT, "Error reading variable information.\n");

    vinfo.bits_per_pixel = 24;
    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres * pages;
    vinfo.yoffset = 0;

    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo))
        LogDebug(VB_CHANNELOUT, "Error setting variable information.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo))
        LogDebug(VB_CHANNELOUT, "Error reading fixed information.\n");

    pagesize = finfo.line_length * vinfo.yres;
    screensize = finfo.smem_len;

    if (!vinfo.pixclock)
        vinfo.pixclock = -1;

    LogDebug(VB_CHANNELOUT, "Original %dx%d, %d bpp, linelen %d, pxclk %d, lrul marg %d %d %d %d, sync len h %d v %d, fps %f\n",
             vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length, vinfo.pixclock,
             vinfo.left_margin, vinfo.right_margin, vinfo.upper_margin, vinfo.lower_margin, vinfo.hsync_len, vinfo.vsync_len,
             (double)(vinfo.xres + vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin) * (vinfo.yres + vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin) / vinfo.pixclock);

    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (!fbfd || (fbp == (char*)-1)) {
        LogDebug(VB_CHANNELOUT, "Failed to mmap.\n");
        return 0;
    }

    // Hide cursor
    int kbfd = open("/dev/console", O_WRONLY);
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_GRAPHICS);
        close(kbfd);
    } else {
        LogErr(VB_CHANNELOUT, "Unable to turn the cursor off\n");
    }

    bool initOK = false;
    if (protocol == "ws2811")
        initOK = InitializeWS281x();

    if (!initOK) {
        LogErr(VB_CHANNELOUT, "Error initializing pixel protocol, Channel Output will be disabled.\n");
        return 0;
    }

    PixelString::AutoCreateOverlayModels(pixelStrings);
    return ChannelOutputBase::Init(config);
}

int DPIPixelsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Close()\n");

    if (fbp)
        munmap(fbp, screensize);

    if (fbfd)
        close(fbfd);

    return ChannelOutputBase::Close();
}

void DPIPixelsOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    PixelString* ps = NULL;
    for (int s = 0; s < stringCount; s++) {
        ps = pixelStrings[s];
        int inCh = 0;
        int min = FPPD_MAX_CHANNELS;
        int max = 0;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < FPPD_MAX_CHANNELS) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
        if (min < max) {
            addRange(min, max);
        }
    }
}

void DPIPixelsOutput::PrepData(unsigned char* channelData) {
    PixelString* ps = NULL;
    long long startTime = 0;
    long long elapsedTimeGather = 0;
    long long elapsedTimeOutput = 0;
    int maxString = stringCount;
    int p = 0;

    if (protocol == "ws2811")
        InitFrameWS281x();

    for (int y = 0; y < longestString; y++) {
        uint32_t rowData[24];

        memset(rowData, 0, sizeof(rowData));

        startTime = GetTime();

        for (int s = 0, ro = 0; s < stringCount; s++) {
            ps = pixelStrings[s];
            if (ps->m_outputChannels && (ps->m_outputChannels / 3) > y) { // Only copy pixel data if string is this long
                rowData[s] =
                    (uint32_t)ps->m_brightnessMaps[p][channelData[ps->m_outputMap[p]]] << 16 |
                    (uint32_t)ps->m_brightnessMaps[p + 1][channelData[ps->m_outputMap[p + 1]]] << 8 |
                    (uint32_t)ps->m_brightnessMaps[p + 2][channelData[ps->m_outputMap[p + 2]]];
            }
        }

        elapsedTimeGather += GetTime() - startTime;

        p += 3;

//#define DEBUG_RAW_GPU_DATA
#ifdef DEBUG_RAW_GPU_DATA
#define ROWDATABYTES(x) (x & 0xFF0000) >> 16, (x & 0x00FF00) >> 8, x & 0xFF
        if (y < 5)
            LogDebug(VB_CHANNELOUT, "y: %02d, rowData: %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x, %02x%02x%02x\n", y,
                     ROWDATABYTES(rowData[0]), ROWDATABYTES(rowData[1]), ROWDATABYTES(rowData[2]), ROWDATABYTES(rowData[3]),
                     ROWDATABYTES(rowData[4]), ROWDATABYTES(rowData[5]), ROWDATABYTES(rowData[6]), ROWDATABYTES(rowData[7]),
                     ROWDATABYTES(rowData[8]), ROWDATABYTES(rowData[9]), ROWDATABYTES(rowData[10]), ROWDATABYTES(rowData[11]));
#endif

        startTime = GetTime();

        if (protocol == "ws2811")
            OutputPixelRowWS281x(rowData, maxString);

        elapsedTimeOutput += GetTime() - startTime;
    }

    // FIXME, clean up these hexdumps after done testing
    //HexDump("fb data:", fbp + (page * pagesize), 216, VB_CHANNELOUT);
    //HexDump("fb 1st line:", fbp + (page * pagesize), finfo.line_length, VB_CHANNELOUT);
    //HexDump("fb 2nd line:", fbp + (page * pagesize) + finfo.line_length, finfo.line_length, VB_CHANNELOUT);
    //HexDump("fb 3rd line:", fbp + (page * pagesize) + (finfo.line_length*2), finfo.line_length, VB_CHANNELOUT);
    //
    //LogDebug(VB_CHANNELOUT, "FB Frame\n");
    //for (int y = 0; y < 3; y++) {
    //    HexDump("Line Begin:", fbp + (page * pagesize) + (finfo.line_length * y), 80, VB_CHANNELOUT);
    //    HexDump("Line End:", fbp + (page * pagesize) + (finfo.line_length * y) + (vinfo.xres * 3 - 80), 80, VB_CHANNELOUT);
    //}

    // FIXME, comment these (and the GetTime() calls above) out once testing is done.
    LogDebug(VB_CHANNELOUT, "Elapsed Time for data gather     : %lldus\n", elapsedTimeGather);
    LogDebug(VB_CHANNELOUT, "Elapsed Time for bit manipulation: %lldus\n", elapsedTimeOutput);
}

int DPIPixelsOutput::SendData(unsigned char* channelData) {
    // Wait for vsync
    int dummy = 0;
    ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);

    // Flip to new page
    vinfo.yoffset = page * vinfo.yres;
    ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);

    // Point to next page.
    page = (page + 1) % pages;

    return m_channelCount;
}

void DPIPixelsOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "FB Device      : %s\n", device.c_str());
    LogDebug(VB_CHANNELOUT, "FB BitsPerPixel: %d\n", vinfo.bits_per_pixel);
    LogDebug(VB_CHANNELOUT, "FB Device          : %s\n", device.c_str());
    LogDebug(VB_CHANNELOUT, "protoBitsPerLine   : %d\n", protoBitsPerLine);
    LogDebug(VB_CHANNELOUT, "xres               : %d\n", vinfo.xres);
    LogDebug(VB_CHANNELOUT, "yres               : %d\n", vinfo.yres);
    LogDebug(VB_CHANNELOUT, "longestString      : %d\n", longestString);

    for (int i = 0; i < stringCount; i++) {
        LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
        pixelStrings[i]->DumpConfig();
    }

    ChannelOutputBase::DumpConfig();
}

int DPIPixelsOutput::GetDPIPinBitPosition(std::string pinName) {
    int bitPosition = -1;

    // clang-format off
                                                   // DPI Pin - GPIO # - Position in RGB888
    if      (pinName == "P1-7")  bitPosition =  7; // DPI_D0  - GPIO4  - 0x010000
    else if (pinName == "P1-29") bitPosition =  6; // DPI_D1  - GPIO5  - 0x020000
    else if (pinName == "P1-31") bitPosition =  5; // DPI_D2  - GPIO6  - 0x040000
    else if (pinName == "P1-26") bitPosition =  4; // DPI_D3  - GPIO7  - 0x080000
    else if (pinName == "P1-24") bitPosition =  3; // DPI_D4  - GPIO8  - 0x100000
    else if (pinName == "P1-21") bitPosition =  2; // DPI_D5  - GPIO9  - 0x200000
    else if (pinName == "P1-19") bitPosition =  1; // DPI_D6  - GPIO10 - 0x400000
    else if (pinName == "P1-23") bitPosition =  0; // DPI_D7  - GPIO11 - 0x800000

    else if (pinName == "P1-32") bitPosition = 15; // DPI_D8  - GPIO12 - 0x000100
    else if (pinName == "P1-33") bitPosition = 14; // DPI_D9  - GPIO13 - 0x000200
    else if (pinName == "P1-8")  bitPosition = 13; // DPI_D10 - GPIO14 - 0x000400
    else if (pinName == "P1-10") bitPosition = 12; // DPI_D11 - GPIO15 - 0x000800
    else if (pinName == "P1-36") bitPosition = 11; // DPI_D12 - GPIO16 - 0x001000
    else if (pinName == "P1-11") bitPosition = 10; // DPI_D13 - GPIO17 - 0x002000
    else if (pinName == "P1-12") bitPosition =  9; // DPI_D14 - GPIO18 - 0x004000
    else if (pinName == "P1-35") bitPosition =  8; // DPI_D15 - GPIO19 - 0x008000

    else if (pinName == "P1-38") bitPosition = 23; // DPI_D16 - GPIO20 - 0x000001
    else if (pinName == "P1-40") bitPosition = 22; // DPI_D17 - GPIO21 - 0x000002
    else if (pinName == "P1-15") bitPosition = 21; // DPI_D18 - GPIO22 - 0x000004
    else if (pinName == "P1-16") bitPosition = 20; // DPI_D19 - GPIO23 - 0x000008
    else if (pinName == "P1-18") bitPosition = 19; // DPI_D20 - GPIO24 - 0x000010
    else if (pinName == "P1-22") bitPosition = 18; // DPI_D21 - GPIO25 - 0x000020
    else if (pinName == "P1-37") bitPosition = 17; // DPI_D22 - GPIO26 - 0x000040
    else if (pinName == "P1-13") bitPosition = 16; // DPI_D23 - GPIO27 - 0x000080
    // clang-format on

    return bitPosition;
}

bool DPIPixelsOutput::FrameBufferIsConfigured(void) {
    if ((fbfd == 0) || (fbfd == -1))
        return false;

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
        LogDebug(VB_CHANNELOUT, "Error reading variable information.\n");

    std::string errStr = "";

    if (protocol == "ws2811") {
        if (vinfo.xres == 362) {
            if (vinfo.yres == 162) {
                if (longestString <= 800)
                    return true;
                else
                    errStr = m_outputType + " Framebuffer configured for 40fps but pixel count is to high.  Reboot is required.";
            } else if (vinfo.yres == 324) {
                if (longestString <= 1600)
                    return true;
                else
                    errStr = m_outputType + " Framebuffer configured for 20fps but pixel count is to high.";
            }
        } else {
            return false;
        }
    }

    if (errStr != "") {
        WarningHolder::AddWarning(errStr);
        errStr += "\n";
        LogErr(VB_CHANNELOUT, errStr.c_str());
    }

    return false;
}

////////////////////////////////////////////////
// Protocol-specific functions
////////////////////////////////////////////////

bool DPIPixelsOutput::InitializeWS281x(void) {
    memset(fbp, 0, pagesize * pages);

    protoBitsPerLine = (vinfo.xres - 2) / 3;                     // Each WS bit is three FB pixels, but skip first/last FB pixel
    protoDestExtra = finfo.line_length - ((vinfo.xres - 2) * 3); // Skip over the hsync/porch/pad area and first/last FB pixel

    protoDest = (uint8_t*)fbp + 3; // Skip the first FB pixel for WS data

    // Setup the first FB row
    for (int x = 0; x < vinfo.xres;) {
        *(protoDest++) = 0xFF;
        *(protoDest++) = 0xFF;
        *(protoDest++) = 0xFF;

        protoDest += 6;
        x += 3;
    }

    // Setup the rest of the FB rows on both pages
    protoDest = (uint8_t*)fbp + finfo.line_length;
    for (int y = 1; y < (vinfo.yres * pages); y++) {
        memcpy(protoDest, fbp, finfo.line_length);
        protoDest += finfo.line_length;
    }

    // Inside OutputPixelRowWS281x(), we'll skip an extra 6 due to the way it works
    protoDestExtra += 6;

    return true;
}

void DPIPixelsOutput::InitFrameWS281x(void) {
    protoBitOnLine = 0;

    // Start at front of page but skip the first && second FB pixels
    // which are 0x000000 & 0xFFFFFF
    protoDest = (uint8_t*)fbp + (page * pagesize) + 3 + 3;
}

void DPIPixelsOutput::OutputPixelRowWS281x(uint32_t* rowData, int maxString) {
    uint32_t onOff = 0;
    int oindex = 0;

    // 24 bits in WS281x output data
    for (int bt = 0; bt < 24; bt++) {
        // 3 FB pixels per WS281x bit.  WS281x 0 == 100, WS281x 1 == 110

        // First FB pixel for the WS bit has already been initialized in InitializeWS281x(),
        // and is skipped over on initialization of protoDest in InitFrameWS281x() and
        // at the bottom of this loop when incrementing protoDest.

        // Second FB pixel for the WS bit
        onOff = 0x000000;

        for (int s = 0; s < maxString; s++) {
            oindex = bitPos[s];
            if (oindex != -1) {
                if (rowData[s] & POSITION_TO_BITMASK(bt))
                    onOff |= POSITION_TO_BITMASK(oindex);
            }
        }
        *(protoDest++) = (onOff >> 16);
        *(protoDest++) = (onOff >> 8);
        *(protoDest++) = (onOff);

        protoBitOnLine++;
        if (protoBitOnLine == protoBitsPerLine) {
            // Jump to beginning of next scan line and reset counter
            protoDest += protoDestExtra;
            protoBitOnLine = 0;
        } else {
            // Third FB pixel for the WS bit is always 0x000000, so jump over it
            //   AND while we're jumping also skip the first FB pixel for the next WS bit.
            protoDest += 6;
        }
    }
}

void DPIPixelsOutput::CompleteFrameWS281x(void) {
}

