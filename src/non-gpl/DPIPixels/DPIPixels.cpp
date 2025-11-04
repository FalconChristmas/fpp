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

#include "../../Sequence.h"
#include "../../Warnings.h"
#include "../../common.h"
#include "../../log.h"
#include "../../settings.h"

#include "DPIPixels.h"
#include "../CapeUtils/CapeUtils.h"
#include "channeloutput/stringtesters/PixelStringTester.h"
#include "util/GPIOUtils.h"

#include "../../overlays/PixelOverlay.h"

#define _0H 1
#define _0L 2
#define _1H 2
#define _1L 1
#define _H(b) ((b) ? _1H : _0H)
#define _L(b) ((b) ? _1L : _0L)

#define POSITION_TO_BITMASK(x) (0x800000 >> (x))

// #define TEST_USING_X11

/*
 * To use this code, the framebuffer must be setup to a specific resolution
 * and timing.  This is normally handled by setupChannelOutputs in
 * /opt/fpp/src/boot/FPPINIT.cpp.  Below are sample configs for config.txt
 *
 * This requires a dtoverlay to set the appropriate clock frequency and
 * resolution and other timing parameters.  The source for the FPP overlay
 * is in /opt/fpp/capes/drivers/pi .  The clock  frequency is differnt for
 * the Pi4 and pre-Pi4's so two dtbo's are generated.
 *
 *
 * Frame Buffer layout:
 * - 362 FB pixels on each scan line (1084 on Pi4)
 * - 3 FB pixels per WS bit (9 on Pi4)
 * - 15 WS channels per scan line
 *
 * FB Pixel #
 * 0       - used only for latch since all RGB888 pins go low during hsync and
 *           we need to turn the latch back on before the first WS bit gets sent
 * 1-8     - WS channel #1
 * 9-16    - WS channel #2
 * ... etc ...
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

    if (displayEnabled && fb) {
        fb->DisableDisplay();
        displayEnabled = false;
    }
    
    if (!m_configuredDpiPins.empty()) {
        for (const auto& pinName : m_configuredDpiPins) {
            try {
                const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
                pin.configPin("gpio", true);
            } catch (...) {
            }
        }
    }

    for (int s = 0; s < stringCount; s++)
        delete pixelStrings[s];

    if (onOffMap)
        free(onOffMap);

    if (fb)
        delete fb;
}

int DPIPixelsOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Init(JSON)\n");

    // Reset flags at start of Init
    displayEnabled = false;
    initComplete = false;
    nonZeroFrameCount = 0;

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

    if (licensedOutputs == 0 && (startsWith(GetFileContents("/proc/device-tree/model"), "Raspberry Pi 5") || startsWith(GetFileContents("/proc/device-tree/model"), "Raspberry Pi Compute Module 5"))) {
        // Pi 5 doesn't have onboard sound but also doesn't work with RPIWS281x.  We'll allow a standard
        // PiHat with two strings to work with DPI on the Pi5.  More than two strings will require a license
        licensedOutputs = 2;
    }

    config["base"] = root;

    // Setup bit offsets for 24 outputs
    for (int i = 0; i < 24; i++) {
        bitPos[i] = -1; // unused pin, no output assigned
    }

    memset(&latchPinMasks, 0, sizeof(latchPinMasks));
    memset(&longestStringInBank, 0, sizeof(longestStringInBank));
    memset(&firstStringInBank, 0, sizeof(firstStringInBank));

    std::vector<std::string> outputPinMap;
    std::string outputList;

    for (int i = 0; i < config["outputs"].size(); i++) {
        if (i >= root["outputs"].size()) {
            continue;
        }
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s, &root["outputs"][i]))
            return 0;

        if (newString->m_outputChannels > longestString)
            longestString = newString->m_outputChannels;

        if (protocol == "")
            protocol = s["protocol"].asString();

        if (root["outputs"][i].isMember("pin")) {
            std::string pinName = root["outputs"][i]["pin"].asString();

            if (pinName[0] == 'P') {
                bitPos[i] = GetDPIPinBitPosition(pinName);
                bitMask[i] = POSITION_TO_BITMASK(bitPos[i]);  // Pre-calculate bitmask

                outputPinMap.push_back(pinName);
            } else {
                outputPinMap.push_back("");
                bitMask[i] = 0;  // No output on this string
            }
        } else if (root["outputs"][i].isMember("sharedOutput")) {
            std::string oPin = outputPinMap[root["outputs"][i]["sharedOutput"].asInt()];
            if (firstStringInBank[1] == 0) {
                firstStringInBank[1] = i;
            } else if (firstStringInBank[2] == 0 && std::count(outputPinMap.begin(), outputPinMap.end(), oPin) == 2) {
                firstStringInBank[2] = i;
            }
            outputPinMap.push_back(oPin);
        } else {
            outputPinMap.push_back("");
        }

        if ((i >= licensedOutputs) && (newString->m_outputChannels > 0)) {
            // apply limit at the source, same code as BBB48StringOutput
            int pixels = 50;
            int chanCount = 0;
            for (auto& a : newString->m_virtualStrings) {
                if (pixels < a.pixelCount) {
                    a.pixelCount = pixels;
                    if (outputList != "") {
                        outputList += ", ";
                    }
                    outputList += std::to_string(i + 1);
                }
                pixels -= a.pixelCount;
                chanCount += a.pixelCount * a.channelsPerNode();
            }
            if (newString->m_isSmartReceiver) {
                chanCount = 0;
                if (outputList != "") {
                    outputList += ", ";
                }
                outputList += std::to_string(i + 1);
            }
            newString->m_outputChannels = chanCount;
        }
        pixelStrings.push_back(newString);
    }
    if (licensedOutputs && root.isMember("latches")) {
        usingLatches = true;

        latchPinMasks[0] = 0;
        latchPinMasks[1] = 0;
        latchPinMasks[2] = 0;

        if (firstStringInBank[2] == 0) {
            firstStringInBank[2] = 52;
        }
        if (firstStringInBank[1] == 0) {
            firstStringInBank[1] = 52;
        }
        for (int l = 0; l < root["latches"].size(); l++) {
            std::string pinName = root["latches"][l].asString();
            if (pinName[0] == 'P') {
                LogExcess(VB_CHANNELOUT, "   Will enable Pin %s for latch\n", pinName.c_str());
                m_configuredDpiPins.push_back(pinName);
                
                const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
                latchPinMasks[l] = POSITION_TO_BITMASK(GetDPIPinBitPosition(pinName));
            }
        }
    }

    while (!pixelStrings.empty() && pixelStrings.back()->m_outputChannels == 0) {
        PixelString* ps = pixelStrings.back();
        delete ps;
        pixelStrings.pop_back();
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
            LogExcess(VB_CHANNELOUT, "   Will enable Pin %s for DPI output (%d pixels)\n",
                      outputPinMap[s].c_str(), pixelCount);
            m_configuredDpiPins.push_back(outputPinMap[s]);

            nonZeroStrings++;
        }
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings (out of %d total) that have 1 or more pixels configured\n",
             nonZeroStrings, stringCount);

    if (stringCount > licensedOutputs) {
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
            std::string warning = "DPIPixels is licensed for ";
            warning += std::to_string(licensedOutputs);
            warning += " outputs, but one or more outputs is configured for more than 50 pixels.  Output(s): ";
            warning += outputList;
            WarningHolder::AddWarning(warning);
            LogWarn(VB_CHANNELOUT, "WARNING: %s\n", warning.c_str());
        }
    }

    if (licensedOutputs) {
        if (usingLatches) {
            for (int s = 0; ((s < stringCount) && (s < firstStringInBank[1])); s++) {
                if (pixelStrings[s]->m_outputChannels > longestStringInBank[0])
                    longestStringInBank[0] = pixelStrings[s]->m_outputChannels;
            }
            for (int s = firstStringInBank[1]; ((s < stringCount) && (s < firstStringInBank[2])); s++) {
                if (pixelStrings[s]->m_outputChannels > longestStringInBank[1])
                    longestStringInBank[1] = pixelStrings[s]->m_outputChannels;
            }
            for (int s = firstStringInBank[2]; ((s < stringCount) && (s < 52)); s++) {
                if (pixelStrings[s]->m_outputChannels > longestStringInBank[2])
                    longestStringInBank[2] = pixelStrings[s]->m_outputChannels;
            }

            // 1 channel padding between splits for latch changes
            longestStringInBank[0]++;
            if (longestStringInBank[2]) {
                longestStringInBank[1]++;
            }

            longestString = 0;
            for (int b = 0; b < MAX_DPI_PIXEL_BANKS; b++)
                longestString += longestStringInBank[b];

            LogDebug(VB_CHANNELOUT, "Running in latch mode, Longest channel counts on each split: %d/%d/%d, total counting padding = %d\n",
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
                        onOffMap[start * stringCount + s] = 0x00; // convert channel offset to pixel offset, store by row
                        ++start;
                    }
                }
            }
        }
    }

    Json::Value fbConfig;
    // Read dimensions from current since we need a config.txt entry to set dpi_timings
    fbConfig["Width"] = 0;
    fbConfig["Height"] = 0;

    // Default is now to use KMS which would be named DPI-1
    device = "DPI-1";
    int card = -1;
    for (int c = 0; c < 10; c++) {
        if (FileExists("/sys/class/drm/card" + std::to_string(c) + "-DPI-1/modes")) {
            card = c;
            break;
        }
    }
    if (card == -1) {
        // fallbacks, may or may not work
        device = "fb1";
        if (!FileExists("/dev/fb1") && FileExists("/dev/fb0")) {
            device = "fb0";
        }
    }
    fbConfig["Device"] = device;
    fbConfig["BitsPerPixel"] = 24;
    fbConfig["Pages"] = 3;

// #define USE_AUTO_SYNC
#ifdef USE_AUTO_SYNC
    fbConfig["AutoSync"] = true;
#endif

#ifdef TEST_USING_X11
    device = "x11";
    fbConfig["Device"] = device;
    fbConfig["Name"] = "DPIPixels";
    fbConfig["Width"] = 362;
    fbConfig["Height"] = 324;
#endif

    fb = FrameBuffer::createFrameBuffer(fbConfig);
    if (fb == nullptr) {
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
    if (protocol == "ws2811") {
        // InitializeWS281x sets up parameters and clears the framebuffer.
        // No template is created anymore - OutputPixelRowWS281x writes all pixels.
        LogInfo(VB_CHANNELOUT, "DPIPixelsOutput::Init calling InitializeWS281x\n");
        initOK = InitializeWS281x();
        LogInfo(VB_CHANNELOUT, "DPIPixelsOutput::Init InitializeWS281x done\n");
    }

    if (!initOK) {
        LogErr(VB_CHANNELOUT, "Error initializing pixel protocol, Channel Output will be disabled.\n");
        WarningHolder::AddWarning("DPIPixels: Error initializing pixel protocol");
        return 0;
    }

    PixelString::AutoCreateOverlayModels(pixelStrings, m_autoCreatedModelNames);
    
    // Write blank WS281x data to framebuffer BEFORE configuring pins as DPI
    // This prevents garbage/flash when pins start outputting
    if (protocol == "ws2811") {
        std::vector<unsigned char> blankData(FPPD_MAX_CHANNELS, 0);
        
        initComplete = true;
        
        for (int page = 0; page < fb->PageCount(); page++) {
            fbPage = page;
            PrepData(blankData.data());
        }
        fbPage = 0;
    }
    
    // Configure DPI pins - they will output blank data from framebuffer
    for (const auto& pinName : m_configuredDpiPins) {
        try {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            pin.configPin("dpi");
        } catch (const std::exception& ex) {
            LogErr(VB_CHANNELOUT, "Failed to configure %s as DPI: %s\n", pinName.c_str(), ex.what());
        }
    }
    
    displayEnabled = false;
    initComplete = true;
    
    return ChannelOutput::Init(config);
}

int DPIPixelsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Close()\n");
    
    // Send blank WS281x data to clear latched pixels before shutdown
    if (fb && stringCount > 0 && protocol == "ws2811") {
        std::vector<unsigned char> blankData(FPPD_MAX_CHANNELS, 0);
        
        bool wasEnabled = displayEnabled;
        for (int page = 0; page < fb->PageCount(); page++) {
            fbPage = page;
            PrepData(blankData.data());
        }
        fbPage = 0;
        
        if (!wasEnabled) {
            fb->EnableDisplay();
            displayEnabled = true;
        }
        
        fb->SyncDisplay(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    if (displayEnabled && fb) {
        fb->DisableDisplay();
        displayEnabled = false;
    }
    
    // Reconfigure DPI pins back to GPIO input
    for (const auto& pinName : m_configuredDpiPins) {
        try {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            pin.configPin("gpio", true);
        } catch (...) {
        }
    }
    m_configuredDpiPins.clear();
    
    for (auto& n : m_autoCreatedModelNames) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(n);
    }
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

void DPIPixelsOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    m_testCycle = cycleNum;
    m_testType = testType;
    m_testPercent = percentOfCycle;

    // We won't overlay the data here because we could have multiple strings
    // pointing at the same channel range so a per-port test cannot
    // be done via channel ranges.  We'll record the test information and use
    // that in prepData
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

    // Check if display should be enabled when significant data arrives
    if (!displayEnabled && fb && initComplete) {
        bool hasSignificantData = (m_testType != 0);
        
        if (!hasSignificantData) {
            int nonZeroChannels = 0;
            for (int s = 0; s < stringCount; s++) {
                ps = pixelStrings[s];
                for (int p = 0; p < ps->m_outputChannels; p++) {
                    int channelIdx = ps->m_outputMap[p];
                    if (channelIdx < FPPD_MAX_CHANNELS && channelData[channelIdx] != 0) {
                        nonZeroChannels++;
                    }
                }
            }
            
            // Require at least 10 non-zero channels for 3 consecutive frames
            if (nonZeroChannels >= 10) {
                nonZeroFrameCount++;
                if (nonZeroFrameCount >= 3) {
                    hasSignificantData = true;
                }
            } else {
                nonZeroFrameCount = 0;
            }
        }
        
        if (hasSignificantData) {
            fb->EnableDisplay();
            displayEnabled = true;
        }
    }

#ifdef USE_AUTO_SYNC
    fbPage = fb->Page(true);
    // LogInfo(VB_CHANNELOUT, "%d - PrepData() checking page\n", fbPage);

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

    uint8_t* outputBuffers[52];
    PixelStringTester* tester = nullptr;
    if (m_testType && m_testCycle >= 0) {
        tester = PixelStringTester::getPixelStringTester(m_testType);
        tester->prepareTestData(m_testCycle, m_testPercent);
    }
    for (int x = 0; x < 52; x++) {
        if (x < stringCount) {
            ps = pixelStrings[x];
            uint32_t newLen = 0;
            outputBuffers[x] = tester
                                   ? tester->createTestData(ps, m_testCycle, m_testPercent, channelData, newLen)
                                   : ps->prepareOutput(channelData);
        } else {
            outputBuffers[x] = nullptr;
        }
    }

    for (int y = 0; y < longestString; y++) {
        uint8_t rowData[24];

        memset(rowData, 0, sizeof(rowData));

        startTime = GetTime();

        if (usingLatches) {
            if (y == 0) {
                ch = 0;
                sStart = 0;
                sEnd = stringCount > firstStringInBank[1] ? firstStringInBank[1] : stringCount;
                maxString = sEnd;
                latchPinMask = latchPinMasks[0];
            } else if (y == longestStringInBank[0]) {
                ch = 0;
                sStart = firstStringInBank[1];
                sEnd = stringCount > firstStringInBank[2] ? firstStringInBank[2] : stringCount;
                maxString = sEnd - sStart;
                latchPinMask = latchPinMasks[1];
            } else if (y == (longestStringInBank[0] + longestStringInBank[1])) {
                ch = 0;
                sStart = firstStringInBank[2];
                sEnd = stringCount > 52 ? 52 : stringCount;
                maxString = std::min(24, sEnd - sStart);
                latchPinMask = latchPinMasks[2];
            }
        }

        for (int s = sStart; s < sEnd; s++) {
            ps = pixelStrings[s];
            if (ps->m_outputChannels) {
                if (ch < ps->m_outputChannels) {
                    rowData[s - sStart] = outputBuffers[s][ch];
                }
            }
        }

        //        elapsedTimeGather += GetTime() - startTime;
        startTime = GetTime();
        if (protocol == "ws2811") {
            OutputPixelRowWS281x(rowData, maxString);
        }
        ++ch;
        //        elapsedTimeOutput += GetTime() - startTime;
    }

    if (protocol == "ws2811")
        CompleteFrameWS281x();

    // FIXME, clean up these hexdumps after done testing
    // uint8_t* fbp = fb->BufferPage(fbPage);
    // HexDump("fb data:", fbp, 216, VB_CHANNELOUT);
    // HexDump("fb 1st line:", fbp, fb->RowStride(), VB_CHANNELOUT);
    // HexDump("fb 2nd line:", fbp + fb->RowStride(), fb->RowStride(), VB_CHANNELOUT);
    // HexDump("fb 3nd line:", fbp + fb->RowStride() * 2, fb->RowStride(), VB_CHANNELOUT);
    // HexDump("fb 11th line:", fbp + fb->RowStride() * 10, fb->RowStride(), VB_CHANNELOUT);

    // FIXME, comment these (and the GetTime() calls above) out once testing is done.
    // LogDebug(VB_CHANNELOUT, "Elapsed Time for data gather     : %lldus\n", elapsedTimeGather);
    // LogDebug(VB_CHANNELOUT, "Elapsed Time for bit manipulation: %lldus\n", elapsedTimeOutput);
}

int DPIPixelsOutput::SendData(unsigned char* channelData) {
#ifdef USE_AUTO_SYNC
    if (fbPage >= 0) {
        // LogInfo(VB_CHANNELOUT, "%d - SendData() marking page dirty\n", fbPage);
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
        LogDebug(VB_CHANNELOUT, "firstStringInBank0 : %d\n", firstStringInBank[0]);
        LogDebug(VB_CHANNELOUT, "firstStringInBank1 : %d\n", firstStringInBank[1]);
        LogDebug(VB_CHANNELOUT, "firstStringInBank2 : %d\n", firstStringInBank[2]);
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
        int currentFPS = getSettingInt("DPI_FPS", 40);
        int newFPS = currentFPS;
        if ((fb->Width() == 362) || (fb->Width() == 1084)) {
            if (fb->Height() == 162) {
                if (longestString <= 2400) {
                    return true;
                } else {
                    errStr = m_outputType + " Framebuffer configured for 40fps but channel count " + std::to_string(longestString) + " is too high.  Reboot is required.";
                    newFPS = 20;
                    std::string nvresults;
                    urlPut("http://127.0.0.1/api/settings/DPI_FPS", "20", nvresults);
                    urlPut("http://127.0.0.1/api/settings/rebootFlag", "1", nvresults);
                }
            } else if (fb->Height() == 324) {
                if (longestString <= 2400) {
                    newFPS = 40;
                    std::string nvresults;
                    urlPut("http://127.0.0.1/api/settings/DPI_FPS", "40", nvresults);
                    urlPut("http://127.0.0.1/api/settings/rebootFlag", "1", nvresults);
                    return true;
                } else if (longestString <= 4800) {
                    return true;
                } else {
                    errStr = m_outputType + " Framebuffer configured for 20fps but channel count " + std::to_string(longestString) + " is too high.";
                }
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
    int sBeginEndSize = 1;

    // Calculate non-latch pins for smart receiver/latch mode
    nonLatchPins = 0xFFFFFF - latchPinMasks[0] - latchPinMasks[1] - latchPinMasks[2];

    // Clear all framebuffer pages to start with a clean slate.
    // The first ws281xResetLines rows will remain zero to provide WS281x RESET period.
    fb->ClearAllPages();

    if (!usingLatches)
        latchPinMask = 0x000000;

    if (fb->Width() == 1084) {
        fbPixelMult = 3;
        sBeginEndSize = 2;
    }

    // Each WS bit is three (or 9 on Pi4) FB pixels, but skip first/last FB pixel
    protoBitsPerLine = (fb->Width() - 2 * sBeginEndSize) / (3 * fbPixelMult);

    // Skip over the hsync/porch pad area and header FB pixels, plus account for
    // the fact that we now write all 3 FB pixels per bit
    protoDestExtra = fb->RowPadding() + (2 * sBeginEndSize * fb->BytesPerPixel());

    LogInfo(VB_CHANNELOUT, "DPIPixelsOutput::InitializeWS281x: Added %d blank lines at top for WS281x RESET\n", ws281xResetLines);

    return true;
}

void DPIPixelsOutput::InitFrameWS281x(void) {
    protoBitOnLine = 0;

    // Start after the WS281x RESET blank lines at the top of the framebuffer.
    // These blank lines (all zeros) provide the >50μs LOW period required for
    // WS281x protocol RESET when the CRTC starts scanning from the top.
    // At ~6.775μs per line (160MHz DPI clock), 10 lines = ~67.75μs of RESET.
    int sBeginEndSize = (fb->Width() == 1084) ? 2 : 1;
    int rowStride = fb->Width() * fb->BytesPerPixel() + fb->RowPadding();
    protoDest = fb->BufferPage(fbPage) + (ws281xResetLines * rowStride) + (sBeginEndSize * fb->BytesPerPixel());
}

void DPIPixelsOutput::OutputPixelRowWS281x(uint8_t* rowData, int maxString) {
    uint32_t onOff = 0;
    uint32_t firstPixel = 0;
    uint32_t lastPixel = 0;
    int oindex = 0;

    // 8 bits in WS281x output data
    for (int bt = 0; bt < 8; bt++) {
        // WS281x encoding: 0-bit = 100 (HIGH-LOW-LOW), 1-bit = 110 (HIGH-HIGH-LOW)
        // We write all 3 FB pixels per WS bit instead of relying on a template.
        
        // First FB pixel: HIGH pulse for all configured outputs (timing)
        // For smart receivers, check onOffMap to see if this pixel position should be active
        if (licensedOutputs) {
            if (usingSmartReceivers) {
                firstPixel = latchPinMask; // Will be 0x000000 when not using latches
                for (int s = 0; s < maxString; s++) {
                    oindex = bitPos[s];
                    if (oindex != -1 && onOffMap) {
                        int strPixel = (protoBitOnLine / 8); // Current pixel in string
                        if (onOffMap[strPixel * stringCount + s]) {
                            firstPixel |= bitMask[s];  // Use pre-calculated bitmask
                        }
                    }
                }
            } else if (usingLatches) {
                firstPixel = latchPinMask | nonLatchPins;
            } else {
                firstPixel = 0xFFFFFF;
            }
        } else {
            firstPixel = latchPinMask; // Will be 0x000000 when not using latches
            for (int s = 0; s < maxString; s++) {
                if (bitPos[s] != -1) {
                    firstPixel |= bitMask[s];  // Use pre-calculated bitmask
                }
            }
        }
        
        // Write first FB pixel (HIGH pulse)
        for (int i = 0; i < fbPixelMult; i++) {
            *(protoDest++) = (firstPixel >> 16);
            *(protoDest++) = (firstPixel >> 8);
            *(protoDest++) = (firstPixel);
            protoDest += fb->BytesPerPixel() - 3;
        }

        // Second/middle FB pixel: HIGH if bit is 1, LOW if bit is 0 (data)
        onOff = latchPinMask; // Will be 0x000000 when not using latches

        for (int s = 0; s < maxString; s++) {
            oindex = bitPos[s];
            if (oindex != -1) {
                if (rowData[s] & (0x80 >> bt))
                    onOff |= bitMask[s];  // Use pre-calculated bitmask
            }
        }

        // Write middle FB pixel (data bit)
        for (int i = 0; i < fbPixelMult; i++) {
            *(protoDest++) = (onOff >> 16);
            *(protoDest++) = (onOff >> 8);
            *(protoDest++) = (onOff);
            protoDest += fb->BytesPerPixel() - 3;
        }

        // Third/last FB pixel: LOW for all outputs (always 0x00 unless latches)
        lastPixel = latchPinMask; // Will be 0x000000 when not using latches
        
        // Write last FB pixel (LOW pulse)
        for (int i = 0; i < fbPixelMult; i++) {
            *(protoDest++) = (lastPixel >> 16);
            *(protoDest++) = (lastPixel >> 8);
            *(protoDest++) = (lastPixel);
            protoDest += fb->BytesPerPixel() - 3;
        }

        protoBitOnLine++;
        if (protoBitOnLine == protoBitsPerLine) {
            // Jump to beginning of next scan line and reset counter
            protoDest += protoDestExtra;
            protoBitOnLine = 0;
        }
        // No else needed - we've already moved protoDest to the right position
    }
}

void DPIPixelsOutput::CompleteFrameWS281x(void) {
}
