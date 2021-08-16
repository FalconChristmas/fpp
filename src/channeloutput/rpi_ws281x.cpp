/*
 *   Raspberry Pi rpi_ws281x handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
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
#include <signal.h>
#include <stdint.h>

#include "rpi_ws281x.h"
#include "util/GPIOUtils.h"

extern "C" {
RPIWS281xOutput* createOutputRPIWS281X(unsigned int startChannel,
                                       unsigned int channelCount) {
    return new RPIWS281xOutput(startChannel, channelCount);
}
}

/////////////////////////////////////////////////////////////////////////////

// Declare ledstring here since it's needed by the CTRL-C handler.
// This could be done other/better ways, but this is simplest for now.

static int ledstringCount = 0;
static ws2811_t ledstring;

/*
 * CTRL-C handler to stop DMA output
 */
static void rpi_ws281x_ctrl_c_handler(int signum) {
    if (ledstringCount) {
        ws2811_fini(&ledstring);
    }
}

/*
 *
 */
RPIWS281xOutput::RPIWS281xOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutputBase(startChannel, channelCount),
    m_spi0(nullptr),
    m_spi1(nullptr),
    m_spi0Data(nullptr),
    m_spi1Data(nullptr),
    m_spi0DataCount(0),
    m_spi1DataCount(0) {
    LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::RPIWS281xOutput(%u, %u)\n",
             startChannel, channelCount);

    offsets[0] = offsets[1] = offsets[2] = offsets[3] = -1;
}

/*
 *
 */
RPIWS281xOutput::~RPIWS281xOutput() {
    LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::~RPIWS281xOutput()\n");

    for (int s = 0; s < m_strings.size(); s++)
        delete m_strings[s];
    if (m_spi0) {
        delete m_spi0;
    }
    if (m_spi1) {
        delete m_spi1;
    }
    if (m_spi0Data) {
        free(m_spi0Data);
    }
    if (m_spi1Data) {
        free(m_spi1Data);
    }
}

/*
 *
 */
int RPIWS281xOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::Init(JSON)\n");

    std::string subType = config["subType"].asString();
    if (subType == "") {
        subType = "PiHat";
    }
    Json::Value root;
    char filename[256];

    std::string verPostf = "";
    if (config["pinoutVersion"].asString() == "2.x") {
        verPostf = "-v2";
    }
    if (config["pinoutVersion"].asString() == "3.x") {
        verPostf = "-v3";
    }
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

    ledstring.freq = 800000; // Hard code this for now
    ledstring.dmanum = 10;

    ledstring.channel[0].count = 0;
    ledstring.channel[0].gpionum = 0;
    ledstring.channel[0].strip_type = WS2811_STRIP_RGB;
    ledstring.channel[0].invert = 0;
    ledstring.channel[0].brightness = 255;

    ledstring.channel[1].count = 0;
    ledstring.channel[1].gpionum = 0;
    ledstring.channel[1].strip_type = WS2811_STRIP_RGB;
    ledstring.channel[1].invert = 0;
    ledstring.channel[1].brightness = 255;

    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString();

        if (!newString->Init(s))
            return 0;

        std::string pinName = root["outputs"][i]["pin"].asString();
        if (pinName[0] == 'P') {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            if (i == 0) {
                ledstring.channel[0].gpionum = pin.gpio;
                ledstring.channel[0].count = newString->m_outputChannels / 3;
                offsets[i] = 0;
                ledstringCount = 1;
            } else {
                ledstring.channel[1].gpionum = pin.gpio;
                ledstring.channel[1].count = newString->m_outputChannels / 3;
                offsets[i] = 1;
                ledstringCount = 1;
            }
        } else {
            //spi
            if (pinName == "spidev0.0") {
                m_spi0 = new SPIUtils(0, 1000000);
                offsets[i] = 2;
                m_spi0Data = (uint8_t*)calloc(newString->m_outputChannels, 1);
                m_spi0DataCount = newString->m_outputChannels;
            } else if (pinName == "spidev0.1") {
                m_spi1 = new SPIUtils(1, 1000000);
                offsets[i] = 3;
                m_spi1Data = (uint8_t*)calloc(newString->m_outputChannels, 1);
                m_spi1DataCount = newString->m_outputChannels;
            }
        }

        m_strings.push_back(newString);
    }

    int gpionum0 = ledstring.channel[0].gpionum;
    if (gpionum0 == 0 || gpionum0 == 13 || gpionum0 == 19) {
        // the pinouts are reversed of what rpi_ws2811 requires.  These GPIO's must be used for string 2
        // we'll flip the two strings and then enable string 1 if needed to get the output to work correctly
        ledstring.channel[0].gpionum = ledstring.channel[1].gpionum ? ledstring.channel[1].gpionum : 18;
        ledstring.channel[1].gpionum = gpionum0;

        std::swap(ledstring.channel[0].count, ledstring.channel[1].count);
        offsets[0] = 1;
        offsets[1] = 0;
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", m_strings.size());

    SetupCtrlCHandler();

    int res = ws2811_init(&ledstring);
    if (res) {
        LogErr(VB_CHANNELOUT, "ws2811_init() failed with error: %d\n", res);
        return 0;
    }
    PixelString::AutoCreateOverlayModels(m_strings);
    return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int RPIWS281xOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::Close()\n");

    ws2811_fini(&ledstring);

    return ThreadedChannelOutputBase::Close();
}

void RPIWS281xOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
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
void RPIWS281xOutput::PrepData(unsigned char* channelData) {
    unsigned char* c = channelData;
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;

    PixelString* ps = NULL;
    int inCh = 0;

    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        inCh = 0;
        if (offsets[s] < 2) {
            for (int p = 0, pix = 0; p < ps->m_outputChannels; pix++) {
                r = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
                g = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
                b = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
                if (offsets[s] == 0) {
                    ledstring.channel[0].leds[pix] = (r << 16) | (g << 8) | (b);
                } else {
                    ledstring.channel[1].leds[pix] = (r << 16) | (g << 8) | (b);
                }
            }
        } else {
            uint8_t* d = offsets[s] == 2 ? m_spi0Data : m_spi1Data;
            for (int p = 0; p < ps->m_outputChannels; p++) {
                d[p] = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
            }
        }
    }
}

/*
 *
 */
int RPIWS281xOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "RPIWS281xOutput::RawSendData(%p)\n", channelData);

    if (ws2811_render(&ledstring)) {
        LogErr(VB_CHANNELOUT, "ws2811_render() failed\n");
    }
    if (m_spi0DataCount) {
        m_spi0->xfer(m_spi0Data, nullptr, m_spi0DataCount);
    }
    if (m_spi1DataCount) {
        m_spi1->xfer(m_spi1Data, nullptr, m_spi1DataCount);
    }

    return m_channelCount;
}

/*
 *
 */
void RPIWS281xOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::DumpConfig()\n");

    for (int i = 0; i < m_strings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
        if (offsets[i] < 2) {
            LogDebug(VB_CHANNELOUT, "      GPIO       : %d\n",
                     ledstring.channel[i].gpionum);
        } else {
            LogDebug(VB_CHANNELOUT, "      SPI        : %d\n",
                     offsets[i] - 2);
        }
        m_strings[i]->DumpConfig();
    }

    ThreadedChannelOutputBase::DumpConfig();
}

void RPIWS281xOutput::SetupCtrlCHandler(void) {
    struct sigaction sa;

    sa.sa_handler = rpi_ws281x_ctrl_c_handler;

    sigaction(SIGKILL, &sa, NULL);
}
