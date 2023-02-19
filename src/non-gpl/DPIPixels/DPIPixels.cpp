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

#include "fpp-pch.h"

#include <vector>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "DPIPixels.h"
#include "../CapeUtils/CapeUtils.h"
#include "util/GPIOUtils.h"

#define _0H 1
#define _0L 2
#define _1H 2
#define _1L 1
#define _H(b) ((b) ? _1H : _0H)
#define _L(b) ((b) ? _1L : _0L)

#define POSITION_TO_BITMASK(x) (0x800000 >> (x))

//#define TEST_USING_X11

/*
 * https://www.raspberrypi.com/documentation/computers/raspberry-pi.html
 * https://www.raspberrypi.com/documentation/computers/config_txt.html
 *
 * To use this code, the framebuffer must be setup to a specific resolution
 * and timing.  This is normally handled by setupChannelOutputs in
 * /opt/fpp/scripts/functions.  Below are sample configs for /boot/config.txt
 *
 * # minimum DPI Pixels FB config.txt entries:
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
 *
 * Frame Buffer layout:
 * - 362 FB pixels on each scan line
 * - 3 FB pixels per WS bit
 * - 24 WS bits per WS pixel
 * - 5 WS pixels per scan line (72 FB pixels per WS pixel)
 *
 * FB Pixel #
 * 0       - used only for latch since all RGB888 pins go low during hsync and
 *           we need to turn the latch back on before the first WS bit gets sent
 * 1-72    - WS pixel #1
 * 73-144  - WS pixel #2
 * 145-216 - WS pixel #3
 * 217-288 - WS pixel #4
 * 289-360 - WS pixel #5
 * 361     - off, unused
 *
 * If latches are used to provide banks of pixels, the latches are turned on in
 * the first FB pixel per scan line if needed and the latch data takes the place
 * of WS bit data in FB pixels where needed.  There is a space equivalent to one
 * blank WS pixel where each latch goes on, this allows us to always have 5 WS
 * pixels per scan line.  Latches turn on in the last FB pixel preceeding the
 * first WS pixel's first bit and stay on through the last FB pixel for the last
 * WS pixel bit in the bank.  The latch pin on a bank is optional if the hat
 * circuitry determines that bank is active because no other banks are active.
 */

#include "Plugin.h"
class DPIPixelsOutputPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    DPIPixelsOutputPlugin() :
        FPPPlugins::Plugin("DPIPixels") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new DPIPixelsOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new DPIPixelsOutputPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

DPIPixelsOutput::DPIPixelsOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::DPIPixelsOutput(%u, %u)\n",
             startChannel, channelCount);
}

DPIPixelsOutput::~DPIPixelsOutput() {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::~DPIPixelsOutput()\n");

    for (int s = 0; s < stringCount; s++)
        delete pixelStrings[s];

    if (onOffMap)
        free(onOffMap);

    if (fb)
        delete fb;
}

int DPIPixelsOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Init(JSON)\n");

    std::string subType = config["subType"].asString();
    if (subType == "") {
        subType = "DPIPixels";
    }
    Json::Value root;
    char filename[256];

    CapeUtils::INSTANCE.initCape(true);

    licensedOutputs = CapeUtils::INSTANCE.getLicensedOutputs();

    if (!CapeUtils::INSTANCE.getStringConfig(subType, root)) {
        LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s\n", subType.c_str());
        WarningHolder::AddWarning("DPIPixels: Could not read pin configuration for " + subType);
        return 0;
    }

    config["base"] = root;

    // Setup bit offsets for 24 outputs
    for (int i = 0; i < 24; i++) {
        bitPos[i] = -1; // unused pin, no output assigned
    }

    memset(&latchPinMasks, 0, sizeof(latchPinMasks));
    memset(&longestStringInBank, 0, sizeof(longestStringInBank));

    std::vector<std::string> outputPinMap;

    for (int i = 0; i < config["outputs"].size(); i++) {
        if (i >= root["outputs"].size()) {
            continue;
        }
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s))
            return 0;

        if ((newString->m_outputChannels / 3) > longestString)
            longestString = newString->m_outputChannels / 3;

        if (protocol == "")
            protocol = s["protocol"].asString();

        if (root["outputs"][i].isMember("pin")) {
            std::string pinName = root["outputs"][i]["pin"].asString();

            if (pinName[0] == 'P') {
                bitPos[i] = GetDPIPinBitPosition(pinName);

                outputPinMap.push_back(pinName);
            } else {
                outputPinMap.push_back("");
            }
        } else if (root["outputs"][i].isMember("sharedOutput")) {
            outputPinMap.push_back(outputPinMap[root["outputs"][i]["sharedOutput"].asInt()]);
        } else {
            outputPinMap.push_back("");
        }

        if ((i >= licensedOutputs) && (newString->m_outputChannels > 0)) {
            // apply limit at the source, same code as BBB48StringOutput
            int pixels = 50;
            int chanCount = 0;
            for (auto& a : newString->m_virtualStrings) {
                if (pixels <= a.pixelCount) {
                    a.pixelCount = pixels;
                }
                pixels -= a.pixelCount;
                chanCount += a.pixelCount * a.channelsPerNode();
            }
            if (newString->m_isSmartReceiver) {
                chanCount = 0;
            }
            newString->m_outputChannels = chanCount;
        }

        if (licensedOutputs && root.isMember("latches")) {
            usingLatches = true;

            latchPinMasks[0] = 0;
            latchPinMasks[1] = 0;
            latchPinMasks[2] = 0;

            for (int l = 0; l < root["latches"].size(); l++) {
                std::string pinName = root["latches"][l].asString();
                if (pinName[0] == 'P') {
                    const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);

                    pin.configPin("dpi");

                    latchPinMasks[l] = POSITION_TO_BITMASK(GetDPIPinBitPosition(pinName));
                }
            }
        }

        pixelStrings.push_back(newString);
    }

    stringCount = pixelStrings.size();

    int nonZeroStrings = 0;
    for (int s = 0; s < stringCount; s++) {
        int pixelCount = 0;
        for (auto& a : pixelStrings[s]->m_virtualStrings) {
            if ((a.receiverNum == -1) && (a.pixelCount > 0))
                pixelCount += a.pixelCount;
        }
        if (pixelCount) {
            LogExcess(VB_CHANNELOUT, "   Enabling Pin %s for DPI output since it has %d pixels configured\n",
                outputPinMap[s].c_str(), pixelCount);

            const PinCapabilities& pin = PinCapabilities::getPinByName(outputPinMap[s]);
            pin.configPin("dpi");

            nonZeroStrings++;
        }
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings (out of %d total) that have 1 or more pixels configured\n",
        nonZeroStrings, stringCount);

    if (stringCount > licensedOutputs) {
        std::string warning;
        std::string outputList;
        int pixels = 0;
        for (int s = 0; s < stringCount; s++) {
            pixels = pixelStrings[s]->m_outputChannels / 3;
            if ((s >= licensedOutputs) && (pixels > 50)) {
                if (outputList != "") {
                    outputList += ", ";
                }

                outputList += std::to_string(s + 1);
            }
        }

        if (outputList != "") {
            warning = "DPIPixels is licensed for ";
            warning += std::to_string(licensedOutputs);
            warning += " outputs, but one or more outputs is configured for more than 50 pixels.  Output(s): ";
            warning += outputList;
            WarningHolder::AddWarning(warning);
            LogWarn(VB_CHANNELOUT, "WARNING: %s\n", warning.c_str());
        }
    }

    if (licensedOutputs) {
        if (usingLatches) {
            for (int s = 0; s < 16; s++) {
                if ((pixelStrings[s]->m_outputChannels / 3) > longestStringInBank[0])
                    longestStringInBank[0] = pixelStrings[s]->m_outputChannels / 3;
            }
            for (int s = 20; ((s < stringCount) && (s < 36)); s++) {
                if ((pixelStrings[s]->m_outputChannels / 3) > longestStringInBank[1])
                    longestStringInBank[1] = pixelStrings[s]->m_outputChannels / 3;
            }

            if (stringCount > 36) {
                for (int s = 36; ((s < stringCount) && (s < 52)); s++) {
                    if ((pixelStrings[s]->m_outputChannels / 3) > longestStringInBank[2])
                        longestStringInBank[2] = pixelStrings[s]->m_outputChannels / 3;
                }
            }

            // 1 pixel padding between splits for latch changes
            longestStringInBank[0]++;
            longestStringInBank[1]++;

            longestString = 0;
            for (int b = 0; b < MAX_DPI_PIXEL_BANKS; b++)
                longestString += longestStringInBank[b];

            LogDebug(VB_CHANNELOUT, "Running in latch mode, Longest Strings on each split: %d/%d/%d, total counting padding = %d\n",
                     longestStringInBank[0], longestStringInBank[1], longestStringInBank[2], longestString);
        }

        onOffMap = (uint8_t*)malloc(longestString * stringCount);
        memset(onOffMap, 0xFF, longestString * stringCount);

        for (int s = 0; s < stringCount && s < licensedOutputs; s++) {
            int start = -1;
            for (auto& a : pixelStrings[s]->m_gpioCommands) {
                if (a.type == 0) {
                    start = a.channelOffset;
                } else if ((a.type == 1) && (start != -1)) {
                    usingSmartReceivers = true;
                    while (start < a.channelOffset) {
                        onOffMap[start / 3 * stringCount + s] = 0x00; // convert channel offset to pixel offset, store by row
                        start += 3;
                    }
                }
            }
        }

        if (usingLatches) {
            // Need to go low for our pad pixels
            for (int s = 0; s < stringCount; s++) {
                if (s < 20)
                    onOffMap[(longestStringInBank[0] - 1) * stringCount + s] = 0x00;
                else if (s < 36)
                    onOffMap[(longestStringInBank[1] - 1) * stringCount + s] = 0x00;
            }
        }
    }

    Json::Value fbConfig;
    fbConfig["Device"] = device;
    fbConfig["BitsPerPixel"] = 24;
    fbConfig["Pages"] = 3;

//#define USE_AUTO_SYNC
#ifdef USE_AUTO_SYNC
    fbConfig["AutoSync"] = true;
#endif

    // Read dimensions from current since we need a /boot/config.txt entry to set dpi_timings
    fbConfig["Width"] = 0;
    fbConfig["Height"] = 0;

#ifdef TEST_USING_X11
    device = "x11";
    fbConfig["Device"] = device;
    fbConfig["Name"] = "DPIPixels";
    fbConfig["Width"] = 362;
    fbConfig["Height"] = 324;
#endif

    fb = new FrameBuffer();
    if (!fb->FBInit(fbConfig)) {
        LogErr(VB_CHANNELOUT, "Error: cannot open FrameBuffer device for %s.\n", device.c_str());
        WarningHolder::AddWarning("DPIPixels: Could not open FrameBuffer device for " + device);
        return 0;
    }

    if (!FrameBufferIsConfigured()) {
        LogErr(VB_CHANNELOUT, "Error: FrameBuffer %s is not configured for DPI Pixels.\n", device.c_str());
        WarningHolder::AddWarning("DPIPixels: FrameBuffer " + device + " is not configured for DPI Pixels");
        return 0;
    }

    LogDebug(VB_CHANNELOUT, "The framebuffer device %s was opened successfully.\n", device.c_str());

    bool initOK = false;
    if (protocol == "ws2811")
        initOK = InitializeWS281x();

    if (!initOK) {
        LogErr(VB_CHANNELOUT, "Error initializing pixel protocol, Channel Output will be disabled.\n");
        WarningHolder::AddWarning("DPIPixels: Error initializing pixel protocol");
        return 0;
    }

    PixelString::AutoCreateOverlayModels(pixelStrings);
    return ChannelOutput::Init(config);
}

int DPIPixelsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Close()\n");

    return ChannelOutput::Close();
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
//    long long elapsedTimeGather = 0;
//    long long elapsedTimeOutput = 0;
    int maxString = stringCount;
    int sStart = 0;
    int sEnd = stringCount;
    int ch = 0;

#ifdef USE_AUTO_SYNC
    fbPage = fb->Page(true);
    //LogInfo(VB_CHANNELOUT, "%d - PrepData() checking page\n", fbPage);

    if (fb->IsDirty(fbPage)) {
        LogErr(VB_CHANNELOUT, "FB Page %d is dirty, output thread isn't keeping up!\n", fbPage);
        fbPage = -1;
        return;
    }

    fb->NextPage(true);
#else
    fbPage = fb->Page();
#endif

    if (protocol == "ws2811")
        InitFrameWS281x();

    for (int y = 0; y < longestString; y++) {
        uint32_t rowData[24];

        memset(rowData, 0, sizeof(rowData));

        startTime = GetTime();

        if (usingLatches) {
            if (y == 0) {
                ch = 0;
                sStart = 0;
                sEnd = stringCount > 20 ? 20 : stringCount;
                maxString = sEnd;
                latchPinMask = latchPinMasks[0];
            } else if (y == longestStringInBank[0]) {
                ch = 0;
                sStart = 20;
                sEnd = stringCount > 36 ? 36 : stringCount;
                maxString = sEnd - 20;
                latchPinMask = latchPinMasks[1];
            } else if (y == (longestStringInBank[0] + longestStringInBank[1])) {
                ch = 0;
                sStart = 36;
                sEnd = stringCount > 52 ? 52 : stringCount;
                maxString = sEnd - 36;
                latchPinMask = latchPinMasks[2];
            }
        }

        for (int s = sStart; s < sEnd; s++) {
            ps = pixelStrings[s];
            if ((ps->m_outputChannels) &&
                (ch < ps->m_outputChannels)) {
                rowData[s - sStart] =
                    (uint32_t)ps->m_brightnessMaps[ch][channelData[ps->m_outputMap[ch]]] << 16 |
                    (uint32_t)ps->m_brightnessMaps[ch + 1][channelData[ps->m_outputMap[ch + 1]]] << 8 |
                    (uint32_t)ps->m_brightnessMaps[ch + 2][channelData[ps->m_outputMap[ch + 2]]];
            }
        }

//        elapsedTimeGather += GetTime() - startTime;

        ch += 3;

        startTime = GetTime();

        if (protocol == "ws2811")
            OutputPixelRowWS281x(rowData, maxString);

//        elapsedTimeOutput += GetTime() - startTime;
    }

    if (protocol == "ws2811")
        CompleteFrameWS281x();

    // FIXME, clean up these hexdumps after done testing
    //uint8_t* fbp = fb->BufferPage(fbPage);
    //HexDump("fb data:", fbp, 216, VB_CHANNELOUT);
    //HexDump("fb 1st line:", fbp, fb->RowStride(), VB_CHANNELOUT);
    //HexDump("fb 2nd line:", fbp + fb->RowStride(), fb->RowStride(), VB_CHANNELOUT);
    //HexDump("fb 3nd line:", fbp + fb->RowStride() * 2, fb->RowStride(), VB_CHANNELOUT);
    //HexDump("fb 11th line:", fbp + fb->RowStride() * 10, fb->RowStride(), VB_CHANNELOUT);

    // FIXME, comment these (and the GetTime() calls above) out once testing is done.
    //LogDebug(VB_CHANNELOUT, "Elapsed Time for data gather     : %lldus\n", elapsedTimeGather);
    //LogDebug(VB_CHANNELOUT, "Elapsed Time for bit manipulation: %lldus\n", elapsedTimeOutput);
}

int DPIPixelsOutput::SendData(unsigned char* channelData) {
#ifdef USE_AUTO_SYNC
    if (fbPage >= 0) {
        //LogInfo(VB_CHANNELOUT, "%d - SendData() marking page dirty\n", fbPage);
        fb->SetDirty(fbPage);
        fbPage = -1;
    }
#else
    fb->SyncDisplay(true);
    fb->NextPage();
#endif

    return m_channelCount;
}

void DPIPixelsOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "protoBitsPerLine   : %d\n", protoBitsPerLine);
    LogDebug(VB_CHANNELOUT, "longestString      : %d\n", longestString);

    if (usingLatches) {
        LogDebug(VB_CHANNELOUT, "longestStringBank0 : %d\n", longestStringInBank[0]);
        LogDebug(VB_CHANNELOUT, "longestStringBank1 : %d\n", longestStringInBank[1]);
        LogDebug(VB_CHANNELOUT, "longestStringBank2 : %d\n", longestStringInBank[2]);
        LogDebug(VB_CHANNELOUT, "LatchPinAMask      : 0x%06x\n", latchPinMasks[0]);
        LogDebug(VB_CHANNELOUT, "LatchPinBMask      : 0x%06x\n", latchPinMasks[1]);
        LogDebug(VB_CHANNELOUT, "LatchPinCMask      : 0x%06x\n", latchPinMasks[2]);
    }

    for (int i = 0; i < stringCount; i++) {
        LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
        pixelStrings[i]->DumpConfig();
    }

    ChannelOutput::DumpConfig();
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
    if (!fb)
        return false;

    std::string errStr = "";

    if (protocol == "ws2811") {
#ifdef TEST_USING_X11
        return true;
#endif
        if ((fb->Width() == 362) || (fb->Width() == 722)) {
            if (fb->Height() == 162) {
                if (longestString <= 800)
                    return true;
                else
                    errStr = m_outputType + " Framebuffer configured for 40fps but pixel count " + std::to_string(longestString) + " is too high.  Reboot is required.";
            } else if (fb->Height() == 324) {
                if (longestString <= 1600)
                    return true;
                else
                    errStr = m_outputType + " Framebuffer configured for 20fps but pixel count " + std::to_string(longestString) + " is too high.";
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
    uint32_t onOff = 0x000000;
    int y = 0;
    int strPixel = 0;
    int sStart = 0;
    int sEnd = stringCount;
    uint32_t nonLatchPins = 0xFFFFFF - latchPinMasks[0] - latchPinMasks[1] - latchPinMasks[2];

    fb->ClearAllPages();

    if (!usingLatches)
        latchPinMask = 0x000000;

    if (fb->Width() == 722)
        fbPixelMult = 2;

    // Each WS bit is three (or 6 on Pi4) FB pixels, but skip first/last FB pixel
    protoBitsPerLine = (fb->Width() - 2) / (3 * fbPixelMult);

    // Skip over the hsync/porch pad area and first/last FB pixel
    protoDestExtra = fb->RowPadding() + (2 * fb->BytesPerPixel());

    // Skip the first FB pixel for WS data
    protoDest = fb->Buffer() + fb->BytesPerPixel();

    while (y < longestString) {
        // For each WS281x pixel on the scan line
        for (int x = 0; x < (protoBitsPerLine / 24); x++) {
            if (usingLatches) {
                if (y == 0) {
                    strPixel = 0;
                    sStart = 0;
                    sEnd = 20;
                    latchPinMask = latchPinMasks[0];
                } else if (y == longestStringInBank[0]) {
                    strPixel = 0;
                    sStart = 20;
                    sEnd = stringCount > 36 ? 36 : stringCount;
                    latchPinMask = latchPinMasks[1];
                } else if (y == (longestStringInBank[0] + longestStringInBank[1])) {
                    strPixel = 0;
                    sStart = 36;
                    sEnd = stringCount > 52 ? 52 : stringCount;
                    latchPinMask = latchPinMasks[2];
                }
            }

            // For each bit in the WS281x pixel protocol
            for (int rp = 0; rp < 24; rp++) {
                if (licensedOutputs) {
                    if (usingLatches && (rp == 0) && ((x == 0) || (strPixel == 0))) {
                        *(protoDest - 3) = (latchPinMask >> 16);
                        *(protoDest - 2) = (latchPinMask >> 8);
                        *(protoDest - 1) = (latchPinMask);
                    }

                    if (usingSmartReceivers) {
                        onOff = latchPinMask; // Will be 0x000000 when not using latches
                        for (int s = sStart; s < sEnd; s++) {
                            if ((bitPos[s - sStart] != -1) && (onOffMap[strPixel * stringCount + s]))
                                onOff |= POSITION_TO_BITMASK(bitPos[s - sStart]);
                        }
                    } else if (usingLatches) {
                        onOff = nonLatchPins | latchPinMask;
                    } else {
                        onOff = 0xFFFFFF;
                    }
                } else {
                    onOff = 0xFFFFFF;
                }

                // Update first FB pixel that makes up the WS281x bit
                // This FB pixel will never be modified outside this routine.
                for (int i = 0; i < fbPixelMult; i++) {
                    *(protoDest++) = (onOff >> 16);
                    *(protoDest++) = (onOff >> 8);
                    *(protoDest++) = (onOff);
                    protoDest += fb->BytesPerPixel() - 3;
                }

                if (usingLatches) {
                    for (int i = 0; i < fbPixelMult; i++) {
                        // Fill in the middle FB pixel even though it will get overwritten
                        // by OutputPixelRowWS281x() later.  It doesn't take much time
                        // doing this once in this init routine, but will be needed if we
                        // ever optimize later to only fill in WS data for pixels that
                        // actually exist.
                        *(protoDest++) = (latchPinMask >> 16);
                        *(protoDest++) = (latchPinMask >> 8);
                        *(protoDest++) = (latchPinMask);
                        protoDest += fb->BytesPerPixel() - 3;
                    }

                    for (int i = 0; i < fbPixelMult; i++) {
                        // Set the latch pin mask on the last FB pixel in the WS bit.
                        // This FB pixel will never be modified outside this routine.
                        *(protoDest++) = (latchPinMask >> 16);
                        *(protoDest++) = (latchPinMask >> 8);
                        *(protoDest++) = (latchPinMask);
                        protoDest += fb->BytesPerPixel() - 3;
                    }
                } else {
                    // Skip over the last two FB pixels.  The middle one
                    // will get updated by OutputPixelRowWS281x() later
                    // and the last one is always 0x000000
                    for (int i = 0; i < fbPixelMult; i++) {
                        protoDest += fb->BytesPerPixel() * 2;
                    }
                }
            }

            y++;
            strPixel++;
        }

        protoDest += protoDestExtra;
    }

    for (int p = 1; p < fb->PageCount(); p++) {
        // Copy first page to rest of pages
        memcpy(fb->Buffer() + (fb->PageSize() * p), fb->Buffer(), fb->PageSize());
    }

    // Inside OutputPixelRowWS281x(), we'll skip an extra 2 FB pixels due to the way it works
    for (int i = 0; i < fbPixelMult; i++) {
        protoDestExtra += fb->BytesPerPixel() * 2;
    }

    return true;
}

void DPIPixelsOutput::InitFrameWS281x(void) {
    protoBitOnLine = 0;

    // Start at front of page but skip the first && second FB pixels
    // which are already populated and don't change
    protoDest = fb->BufferPage(fbPage) + ((1 + fbPixelMult) * fb->BytesPerPixel());
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
        onOff = latchPinMask; // Will be 0x000000 when not using latches;

        for (int s = 0; s < maxString; s++) {
            oindex = bitPos[s];
            if (oindex != -1) {
                if (rowData[s] & POSITION_TO_BITMASK(bt))
                    onOff |= POSITION_TO_BITMASK(oindex);
            }
        }

        for (int i = 0; i < fbPixelMult; i++) {
            *(protoDest++) = (onOff >> 16);
            *(protoDest++) = (onOff >> 8);
            *(protoDest++) = (onOff);
#ifdef TEST_USING_X11
            // This is just a slowdown and unneeded when not testing
            protoDest += fb->BytesPerPixel() - 3;
#endif
        }

        protoBitOnLine++;
        if (protoBitOnLine == protoBitsPerLine) {
            // Jump to beginning of next scan line and reset counter
            protoDest += protoDestExtra;
            protoBitOnLine = 0;
        } else {
            // Third FB pixel for the WS bit and first FB pixel for next WS bit
            // are already set and don't change, so jump over them.
#ifdef TEST_USING_X11
            // This is just a slowdown and static when not testing
            protoDest += fb->BytesPerPixel() * 2 * fbPixelMult;
#else
            protoDest += 6 * fbPixelMult;
#endif
        }
    }
}

void DPIPixelsOutput::CompleteFrameWS281x(void) {
}
