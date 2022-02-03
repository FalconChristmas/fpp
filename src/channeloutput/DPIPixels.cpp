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

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "DPIPixels.h"
#include "util/GPIOUtils.h"

#define _0H  1
#define _0L  2
#define _1H  2
#define _1L  1
#define _H(b)  ((b)? _1H: _0H)
#define _L(b)  ((b)? _1L: _0L)

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
    ThreadedChannelOutputBase(startChannel, channelCount),
    device("/dev/fb0"),
    protocol("ws2811") {

    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::DPIPixelsOutput(%u, %u)\n",
             startChannel, channelCount);
}

DPIPixelsOutput::~DPIPixelsOutput() {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::~DPIPixelsOutput()\n");

    for (int s = 0; s < m_strings.size(); s++)
        delete m_strings[s];
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

        std::string pinName = root["outputs"][i]["pin"].asString();

        if (pinName[0] == 'P') {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);

            pin.configPin("dpi"); // Enable ALT2 functionality for this pin

            if      (pinName == "P1-7")  bitPos[i] =  7; // DPI_D0
            else if (pinName == "P1-29") bitPos[i] =  6; // DPI_D1
            else if (pinName == "P1-31") bitPos[i] =  5; // DPI_D2
            else if (pinName == "P1-26") bitPos[i] =  4; // DPI_D3
            else if (pinName == "P1-24") bitPos[i] =  3; // DPI_D4
            else if (pinName == "P1-21") bitPos[i] =  2; // DPI_D5
            else if (pinName == "P1-19") bitPos[i] =  1; // DPI_D6
            else if (pinName == "P1-23") bitPos[i] =  0; // DPI_D7

            else if (pinName == "P1-32") bitPos[i] = 15; // DPI_D8
            else if (pinName == "P1-33") bitPos[i] = 14; // DPI_D9
            else if (pinName == "P1-8")  bitPos[i] = 13; // DPI_D10
            else if (pinName == "P1-10") bitPos[i] = 12; // DPI_D11
            else if (pinName == "P1-36") bitPos[i] = 11; // DPI_D12
            else if (pinName == "P1-11") bitPos[i] = 10; // DPI_D13
            else if (pinName == "P1-12") bitPos[i] =  9; // DPI_D14
            else if (pinName == "P1-35") bitPos[i] =  8; // DPI_D15

            else if (pinName == "P1-38") bitPos[i] = 23; // DPI_D16
            else if (pinName == "P1-40") bitPos[i] = 22; // DPI_D17
            else if (pinName == "P1-15") bitPos[i] = 21; // DPI_D18
            else if (pinName == "P1-16") bitPos[i] = 20; // DPI_D19
            else if (pinName == "P1-18") bitPos[i] = 19; // DPI_D20
            else if (pinName == "P1-22") bitPos[i] = 18; // DPI_D21
            else if (pinName == "P1-37") bitPos[i] = 17; // DPI_D22
            else if (pinName == "P1-13") bitPos[i] = 16; // DPI_D23
        }

        m_strings.push_back(newString);
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", m_strings.size());

    // Open the Frame Buffer for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (!fbfd || ((int)fbfd == -1))
    {
        LogDebug(VB_CHANNELOUT, "Error: cannot open framebuffer device.\n");
        return 0;
    }

    if (!FrameBufferIsConfigured() && FileExists("/dev/fb1")) {
        // Try /dev/fb1 if /dev/fb0 isn't configured
        close(fbfd);

        LogDebug(VB_CHANNELOUT, "/dev/fb0 is not configured for DPI Pixels, trying /dev/fb1\n");

        device = "/dev/fb1";
        fbfd = open("/dev/fb1", O_RDWR);
        if (!fbfd || ((int)fbfd == -1))
        {
            LogDebug(VB_CHANNELOUT, "Error: /dev/fb0 is not configured and can not open /dev/fb1\n");
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
    vinfo.yres_virtual = vinfo.yres * 2;
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
       (double)(vinfo.xres + vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin) * (vinfo.yres + vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin ) / vinfo.pixclock);

    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (!fbfd || (fbp == (char *)-1)) {
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

    PixelString::AutoCreateOverlayModels(m_strings);
    return ThreadedChannelOutputBase::Init(config);
}

int DPIPixelsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Close()\n");

    if (fbp)
        munmap(fbp, screensize);

    if (fbfd)
        close(fbfd);

    return ThreadedChannelOutputBase::Close();
}

void DPIPixelsOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    PixelString* ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
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

    if (protocol == "ws2811")
        InitFrameWS281x();

    for (int y = 0, p = 0; y < longestString; y++) {
        uint32_t rowData[24];

        memset(rowData, 0, sizeof(rowData));

        startTime = GetTime();

        for (int s = 0, ro = 0; s < m_strings.size(); ++s) {
            ps = m_strings[s];
            if (ps->m_outputChannels && (ps->m_outputChannels / 3) > y) { // Only copy pixel data if string is this long
                rowData[s] =
                    (uint32_t)ps->m_brightnessMaps[p  ][channelData[ps->m_outputMap[p  ]]] << 16 |
                    (uint32_t)ps->m_brightnessMaps[p+1][channelData[ps->m_outputMap[p+1]]] << 8 |
                    (uint32_t)ps->m_brightnessMaps[p+2][channelData[ps->m_outputMap[p+2]]];
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
            OutputPixelRowWS281x(rowData);

        elapsedTimeOutput += GetTime() - startTime;
    }

//    HexDump("fb data:", fbp + (page * pagesize), 216, VB_CHANNELOUT);

    // FIXME, set these to LogExcess once testing is done.
    LogDebug(VB_CHANNELOUT, "Elapsed Time for data gather     : %lldus\n", elapsedTimeGather);
    LogDebug(VB_CHANNELOUT, "Elapsed Time for bit manipulation: %lldus\n", elapsedTimeOutput);

    page = page ? 0 : 1;
}

int DPIPixelsOutput::RawSendData(unsigned char* channelData) {
    // Flip to current page
    vinfo.yoffset = page * vinfo.yres;
    ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);

    return m_channelCount;
}

void DPIPixelsOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "FB Device      : %s\n", device.c_str());
    LogDebug(VB_CHANNELOUT, "FB BitsPerPixel: %d\n", vinfo.bits_per_pixel);

    for (int i = 0; i < m_strings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
        m_strings[i]->DumpConfig();
    }

    ThreadedChannelOutputBase::DumpConfig();
}

bool DPIPixelsOutput::FrameBufferIsConfigured(void) {
    if ((fbfd == 0) || (fbfd == -1))
        return false;

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
      LogDebug(VB_CHANNELOUT, "Error reading variable information.\n");

    std::string errStr = "";

    if (protocol == "ws2811") {
        if (vinfo.xres == 360) {
            if (vinfo.yres == 162) {
                if (longestString <= 800)
                    return true;
                else
                    errStr = "DPIPixels Framebuffer configured for 40fps but pixel count is to high.  Reboot is required.";
            } else if (vinfo.yres == 324) {
                return true;
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

void DPIPixelsOutput::InitFrameWS281x(void) {
    protoBitsPerLine = vinfo.xres / 3; // Each WS bit is one FB pixel
    protoBitOnLine = 0;

    protoDest = (uint8_t*)fbp + (page * pagesize);

    memset(protoDest, 0, pagesize);
}

void DPIPixelsOutput::OutputPixelRowWS281x(uint32_t *rowData) {
    uint32_t onOff = 0;
    uint32_t bv;
    int oindex = 0;
    int destExtra = finfo.line_length - (vinfo.xres * 3); // Skip over the hsync/porch/pad area

    // 24 bits in WS281x output data
    for (int bt = 0; bt < 24; ++bt) {
        // 3 FB pixels per WS281x bit.  WS281x 0 == 100, WS281x 1 == 110

        // First FB pixel for the WS bit
        if (1) { // no smart receivers
            *(protoDest++) = 0xFF;
            *(protoDest++) = 0xFF;
            *(protoDest++) = 0xFF;
        } else { // FIXME, will have to handle smart receivers here
            onOff = 0xFFFFFF;
            *(protoDest++) = (onOff >> 16);
            *(protoDest++) = (onOff >>  8);
            *(protoDest++) = (onOff      );
        }

        // Second FB pixel for the WS bit
        onOff = 0x000000;
        for (int s = 0; s < m_strings.size(); ++s) {
            oindex = bitPos[s];
            if (oindex != -1) {
                if (rowData[s] & (0x800000 >> bt))
                    onOff |= 0x800000 >> oindex;
            }
        }
        *(protoDest++) = (onOff >> 16);
        *(protoDest++) = (onOff >>  8);
        *(protoDest++) = (onOff      );

        // Third FB pixel for the WS bit is always 0, so jump over it
        protoDest += 3;

        protoBitOnLine++;
        if (protoBitOnLine == protoBitsPerLine) {
            // Jump to beginning of next scan line and reset counter
            protoDest += destExtra;
            protoBitOnLine = 0;
        }
    }
}

