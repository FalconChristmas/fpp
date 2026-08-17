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

#include "fpp-json.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>

#include "../../Sequence.h"
#include "../../Warnings.h"
#include "../../common.h"
#include "../../log.h"
#include "../../settings.h"

#include "DPIPixels.h"
#include "../CapeUtils/CapeUtils.h"
#include "channeloutput/channeloutputthread.h"
#include "channeloutput/stringtesters/PixelStringTester.h"
#include "util/GPIOUtils.h"

#include "../../overlays/PixelOverlay.h"

#define _0H 1
#define _0L 2
#define _1H 2
#define _1L 1
#define _H(b) ((b) ? _1H : _0H)
#define _L(b) ((b) ? _1L : _0L)

#define POSITION_TO_BITMASK(x) (0x000001 << (x))

// Uncomment to log elapsed time in PrepData()
// #define LOG_ELAPSED_TIME
// Uncomment to enable the HSync (P1-5) and VSync (P1-3) pins for logic analyzing
// WARNING: THIS WILL BREAK I2C ON THE DEVICE SINCE THE HSYNC/VSYNC PINS ARE SHARED WITH I2C
// #define LOGIC_ANALYZING

/*
 * New DPI config as of 2026.  Rather than allowing a max of 800 or 1600
 * pixels per string and having banks share the max of 800 or 1600, the
 * DPI clock is now set to a much higher rate allowing the banks to be
 * interleaved allowing up to 80 outputs using 4 latch pins.  For boards
 * supporting the standard 16-output headers, this would allow up to 64
 * string outputs each supporting 800 or 1600 pixels at full frame rate.
 *
 * To use this code, the framebuffer must be setup to a specific resolution
 * and timing.  This is normally handled by setupChannelOutputs in
 * /opt/fpp/src/boot/FPPINIT.cpp.
 *
 * This requires a dtoverlay to set the appropriate clock frequency and
 * resolution and other timing parameters.  The source for the FPP overlay
 * is in /opt/fpp/capes/drivers/pi.  The overlay only needs to establish the
 * 1920-wide, 38.4MHz DPI pipeline; its vertical resolution is just a boot
 * default and is overridden at runtime (see below).
 *
 * The width is 1920 and the pixel clock 38.4MHz, which makes each WS bit 48 FB
 * pixels (1.25us).  The vertical resolution is sized at Init() to the longest
 * configured string: each scanline carries 5 channels, so the data occupies
 * ceil(longestString/5) scanlines plus a small blank reset tail.  The output
 * frame rate is then set per-sequence to min(sequenceRate, maxRateForLength) by
 * varying ONLY the vertical blanking (vtotal) via FrameBuffer::SetRefreshRate();
 * on the Pi5/RP1 DPI this is a runtime modeset that needs no reboot.  The
 * clock stays fixed so the WS bit timing and framebuffer layout never change.
 * fps * maxChannels is bounded by the WS281x 800kHz bit rate (~96000), e.g. at
 * 38.4MHz: 40fps<->2400ch, 60fps<->1600ch, 80fps<->1200ch, 100fps<->960ch.
 *
 * 48 FrameBuffer pixels are used to drive each WS bit for all channels.  This
 * allows 16 FB pixels to set the initial On state, 16 FB pixels that are On/Off
 * to set the WS bit on or off, and the trailing 16 FB pixels are off
 * for the trailing third of the WS bit timing.  When using latches, the
 * 16 FB pixels allow turning up to 4 latches on/off while still
 * honoring the WS protocol timing.  The different groups of pixels on
 * each latched output are offset by only 104 microseconds from each
 * other rather than being in banks as they previously were meaning
 * bank 3 would be sent after all the pixels for banks 0-2 were sent.
 *
 * The latching process is as follows (time offset in nanoseconds(ns)):
 *
 * 000ns - FB pixel 0 sets the data bits for latch #0's pixels
 * 026ns - FB pixel 1 holds the data bits and turns latch #0 pin on
 * 052ns - FB pixel 2 holds the data bits and turns latch #0 pin off
 * 078ns - FB pixel 3 holds the data bits
 * 104ns - FB pixel 4 sets the data bits for latch #1's pixels
 * 130ns - FB pixel 5 holds the data bits and turns latch #1 pin on
 * 156ns - FB pixel 6 holds the data bits and turns latch #1 pin off
 * 182ns - FB pixel 7 holds the data bits
 * 208ns - FB pixel 8 sets the data bits for latch #2's pixels
 * ...
 * 416ns - FB pixel 16 sets the data bits for latch #0's pixels (2nd WS bit)
 * ...
 *
 * This sequence allows the data bits to be set 26ns before the
 * latch-enable bits are turned on and the data bits are not changed
 * until at least 26ns after the latch-enable bits are turned off.
 * Newer latch chips do not need anywhere near this time and the timing
 * still works with older chips still in use.
 *
 * This same spacing of 48 FB pixels per WS bit is still used when
 * latch chips are not in use, but since there is no latching, the
 * first 16 FB pixels are identical to each other, the second 16 are
 * identical to each other, and the last 16 FB pixels are identical
 * to each other.
 *
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

    if (!m_configuredDPIPins.empty()) {
        for (const auto& pinName : m_configuredDPIPins) {
            try {
                const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
                pin.releasePin();
            } catch (...) {
            }
        }
    }

    for (auto a : pixelStrings) {
        delete a;
    }
    pixelStrings.clear();

    if (onOffMap)
        free(onOffMap);

    if (fb)
        delete fb;
}

// The RPIWS281X driver has been removed and its outputs are now driven by DPI.  Most
// RPIWS281X capes ship a DPIPixels variant of their string config in the same EEPROM
// with an identical pin list, so prefer that.  rPi-28D is special cased: its RPIWS281X
// config's third output is ws2801 over SPI which DPI cannot drive, so it maps to the
// 4-output variant that repurposes CN1's clock/data pins as WS281x outputs.
static std::map<std::string, std::string> RPIWS281X_SUBTYPE_MAP = {
    { "PiHat", "PiHat-DPIPixels" },
    { "rPi-MFC", "rPi-MFC-DPIPixels" },
    { "rPi-28D", "rPi-28D-DPIPixels-4" }
};

bool DPIPixelsOutput::LoadStringConfig(const std::string& subType, Json::Value& root, std::string& usedSubType) {
    auto it = RPIWS281X_SUBTYPE_MAP.find(subType);
    if ((it != RPIWS281X_SUBTYPE_MAP.end()) && CapeUtils::INSTANCE.getStringConfig(it->second, root)) {
        usedSubType = it->second;
        return true;
    }
    // A cape we don't know about.  If it ships a "-DPIPixels" variant, use it.
    if (!endsWith(subType, "-DPIPixels") && CapeUtils::INSTANCE.getStringConfig(subType + "-DPIPixels", root)) {
        usedSubType = subType + "-DPIPixels";
        return true;
    }
    // Fall back to the config as named.  For a third-party cape with a physically burned
    // EEPROM this may be an RPIWS281X config, which uses the same pin format we do.
    if (CapeUtils::INSTANCE.getStringConfig(subType, root)) {
        usedSubType = subType;
        return true;
    }
    return false;
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

    std::string usedSubType;
    if (!LoadStringConfig(subType, root, usedSubType)) {
        LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s\n", subType.c_str());
        WarningHolder::AddWarning(13, "DPIPixels: Could not read pin configuration for " + subType);
        return 0;
    }
    if (usedSubType != subType) {
        LogInfo(VB_CHANNELOUT, "Mapped string config %s to %s for DPI output\n",
                subType.c_str(), usedSubType.c_str());
    }

    if (licensedOutputs == 0) {
        // Two strings driven directly off the Pi's GPIO header is what the old RPIWS281X
        // driver provided for free, so DPI has to keep providing it.  More than two
        // outputs still requires a license.
        licensedOutputs = 2;
    }

    config["base"] = root;

    // A cape with a physical EEPROM has every pin hard-wired to an output buffer, so all of
    // its pins are muxed for DPI even when a port is left empty, otherwise the buffer input
    // would float.  A virtual EEPROM describes a hand-wired setup where the unused ports may
    // not be connected to anything, so only the pins for ports with channels configured are
    // claimed, leaving the rest available for other uses such as GPIO inputs.
    const Json::Value& capeInfo = CapeUtils::INSTANCE.getCapeInfo();
    bool virtualEEPROM = capeInfo["eepromLocation"].asString().find("sys/bus/i2c") == std::string::npos;

    for (int i = 0; i < (MAX_DPI_PIXEL_LATCHES * 24); i++) {
        bitPos[i] = -1; // unused pin, no output assigned
        outputToStringMap[i] = -1;
        stringToOutputMap[i] = -1;
    }

    std::vector<std::string> outputPinMap;
    std::vector<int> outputChannelCounts;
    std::string outputList;
    std::string skippedOutputs;
    int firstStringInBank[MAX_DPI_PIXEL_LATCHES];

    memset(latchPinMasks, 0, sizeof(latchPinMasks));
    memset(stringLengths, 0, sizeof(stringLengths));
    memset(firstStringInBank, 0, sizeof(firstStringInBank));

    for (int i = 0; i < config["outputs"].size(); i++) {
        if (i >= root["outputs"].size()) {
            continue;
        }
        Json::Value s = config["outputs"][i];
        PixelString* newString = new PixelString(true);

        if (!newString->Init(s, &root["outputs"][i]))
            return 0;

        if (newString->m_outputBytes > longestString)
            longestString = newString->m_outputBytes;

        if (protocol == "")
            protocol = s["protocol"].asString();

        outputChannelCounts.push_back(newString->m_outputBytes);

        if (root["outputs"][i].isMember("pin")) {
            std::string pinName = root["outputs"][i]["pin"].asString();
            int pinBitPos = startsWith(pinName, "P") ? GetDPIPinBitPosition(pinName) : -1;

            if (pinBitPos >= 0) {
                bitPos[i] = pinBitPos;
                outputToStringMap[pinBitPos] = i;
                stringToOutputMap[i] = pinBitPos;
                stringLengths[pinBitPos] = newString->m_outputBytes;

                outputPinMap.push_back(pinName);
            } else {
                // A pin DPI cannot drive.  The old RPIWS281X configs put ws2801 strings on
                // spidev pins, so skip just that output rather than failing the whole cape.
                if (newString->m_outputBytes > 0) {
                    if (skippedOutputs != "") {
                        skippedOutputs += ", ";
                    }
                    skippedOutputs += std::to_string(i + 1) + " (" + pinName + ")";
                }
                outputPinMap.push_back("");
            }
        } else if (root["outputs"][i].isMember("sharedOutput")) {
            std::string pinName = outputPinMap[root["outputs"][i]["sharedOutput"].asInt()];
            bitPos[i] = GetDPIPinBitPosition(pinName);
            for (int latch = 1; latch < MAX_DPI_PIXEL_LATCHES; latch++) {
                if ((firstStringInBank[latch] == 0) &&
                    (std::count(outputPinMap.begin(), outputPinMap.end(), pinName) == latch)) {
                    firstStringInBank[latch] = i;
                    latchCount = latch + 1;
                }
            }

            for (int z = MAX_DPI_PIXEL_LATCHES - 1; z > 0; z--) {
                if (firstStringInBank[z] != 0) {
                    outputToStringMap[bitPos[i] + (z * 24)] = i;
                    stringToOutputMap[i] = bitPos[i] + (z * 24);
                    stringLengths[bitPos[i] + (z * 24)] = newString->m_outputBytes;
                    z = 0;
                }
            }

            outputPinMap.push_back(pinName);
        } else {
            outputPinMap.push_back("");
        }

        if ((i >= licensedOutputs) && (newString->m_outputBytes > 0)) {
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
            newString->setPixelDataChannels(chanCount);
        }
        pixelStrings.push_back(newString);
    }

    if (skippedOutputs != "") {
        std::string warning = "DPIPixels: output(s) " + skippedOutputs +
                              " cannot be driven by DPI and have been disabled.  Only pixels on the Pi's DPI pins are supported.";
        WarningHolder::AddWarning(warning);
        LogWarn(VB_CHANNELOUT, "WARNING: %s\n", warning.c_str());
    }

    for (int i = 0; i < outputPinMap.size(); i++) {
        const std::string& pinName = outputPinMap[i];
        if (pinName.empty())
            continue;
        if (virtualEEPROM && (outputChannelCounts[i] == 0))
            continue;
        // Outputs sharing a pin via latches can each map back to the same pin
        if (std::find(m_configuredDPIPins.begin(), m_configuredDPIPins.end(), pinName) != m_configuredDPIPins.end())
            continue;

        LogExcess(VB_CHANNELOUT, "   Will enable Pin %s for DPI output\n", pinName.c_str());
        m_configuredDPIPins.push_back(pinName);
    }

    if (licensedOutputs && root.isMember("latches")) {
        usingLatches = true;

        if (root["latches"].size() > 4) {
            LogErr(VB_CHANNELOUT, "Error, DPIPixels only supports 4 latch pins, %d configured by cape.\n",
                   root["latches"].size());
            WarningHolder::AddWarning("DPIPixels: Max of 4 latch pins supported, but " +
                                      std::to_string(root["latches"].size()) + "configured by cape.");
            return 0;
        }

        for (int l = 0; l < root["latches"].size(); l++) {
            std::string pinName = root["latches"][l].asString();
            if (pinName[0] == 'P') {
                LogExcess(VB_CHANNELOUT, "Will enable Pin %s for latch\n", pinName.c_str());
                m_configuredDPIPins.push_back(pinName);
                latchPinMasks[l] = POSITION_TO_BITMASK(GetDPIPinBitPosition(pinName));
            }
        }
    } else {
        // FIXME, shouldn't need this line, remove after testing without latch chips
        // latchCount = 1;
    }

    while (!pixelStrings.empty() && pixelStrings.back()->m_outputBytes == 0) {
        PixelString* ps = pixelStrings.back();
        delete ps;
        pixelStrings.pop_back();
    }

    int nonZeroStrings = 0;
    for (int s = 0; s < pixelStrings.size(); s++) {
        int pixelCount = 0;
        for (auto& a : pixelStrings[s]->m_virtualStrings) {
            if ((a.receiverNum == -1) && (a.pixelCount > 0))
                pixelCount += a.pixelCount;
        }
        if (pixelCount) {
            nonZeroStrings++;
        }
    }

    LogDebug(VB_CHANNELOUT, "   Found %d strings (out of %d total) that have 1 or more pixels configured\n",
             nonZeroStrings, pixelStrings.size());

    if (pixelStrings.size() > licensedOutputs) {
        int pixels = 0;
        for (int s = 0; s < pixelStrings.size(); s++) {
            pixels = pixelStrings[s]->m_outputBytes / 3;
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
        for (int lp = 0; lp < MAX_DPI_PIXEL_LATCHES; lp++) {
            for (int p = 0; p < 4800; p++) {
                onOffMask[lp][p] = 0xFFFFFF;
            }
        }

        onOffMap = (uint8_t*)malloc(longestString * pixelStrings.size());
        memset(onOffMap, 0xFF, longestString * pixelStrings.size());

        for (int s = 0; s < pixelStrings.size() && s < licensedOutputs; s++) {
            int start = -1;
            for (auto& a : pixelStrings[s]->m_gpioCommands) {
                if (a.type == 0) {
                    start = a.channelOffset;
                } else if ((a.type == 1) && (start != -1)) {
                    usingSmartReceivers = true;

                    int lp = stringToOutputMap[s] / 24;

                    while (start < a.channelOffset) {
                        // Turn Off bits in the mask as needed
                        onOffMask[lp][start] &= ~(POSITION_TO_BITMASK(stringToOutputMap[s] % 24));
                        onOffMap[start * pixelStrings.size() + s] = 0x00; // convert channel offset to pixel offset, store by row
                        ++start;
                    }
                }
            }
        }
    }

    // Size the framebuffer height to the longest string.  At the fixed DPI
    // geometry (1920 wide, 48 FB pixels per WS bit) each scanline carries 5
    // channels, so the WS bit stream for the longest string occupies
    // ceil(longestString / 5) scanlines.  A few trailing blank scanlines
    // guarantee the >50us WS reset low between frames.  The frame rate is then
    // set at runtime purely by the vertical blanking (see PrepData ->
    // FrameBuffer::SetRefreshRate), so this height is fixed for the life of the
    // output and never needs a reboot to change fps.
    constexpr int DPI_CHANNELS_PER_ROW = 5;   // 1920 / (3 * 16 fbPixelMult) / 8 bits
    constexpr int DPI_RESET_ROWS = 8;         // ~400us guaranteed-low reset tail
    constexpr int DPI_MAX_CHANNELS = 4800;    // onOffMask[][4800] limit (1600 pixels)
    int cappedChannels = std::min(longestString, DPI_MAX_CHANNELS);
    int dataRows = (cappedChannels + DPI_CHANNELS_PER_ROW - 1) / DPI_CHANNELS_PER_ROW;
    int neededRows = dataRows + DPI_RESET_ROWS;

    Json::Value fbConfig;
    // Width 0 => use the connector's (1920) width.  Height is our computed row
    // count; VariableRefresh tells KMS to drive its own vertical timing so we can
    // change the refresh rate at runtime without a reboot.
    fbConfig["Width"] = 0;
    fbConfig["Height"] = neededRows;
    fbConfig["VariableRefresh"] = true;

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
//    fbConfig["Pages"] = 3;

// FIXME - Auto sync is currently consuming 100% of a core, need to figure this out.
// #define USE_AUTO_SYNC
#ifdef USE_AUTO_SYNC
    fbConfig["AutoSync"] = true;
#endif

    fb = FrameBuffer::createFrameBuffer(fbConfig);
    if (fb == nullptr) {
        LogErr(VB_CHANNELOUT, "Error: cannot open FrameBuffer device for %s.\n", device.c_str());
        WarningHolder::AddWarning(13, "DPIPixels: Could not open FrameBuffer device for " + device);
        return 0;
    }

    if (!FrameBufferIsConfigured()) {
        LogErr(VB_CHANNELOUT, "Error: FrameBuffer %s is not configured for DPI Pixels.\n", device.c_str());
        WarningHolder::AddWarning(13, "DPIPixels: FrameBuffer " + device + " is not configured for DPI Pixels");
        return 0;
    }

    LogDebug(VB_CHANNELOUT, "The framebuffer device %s was opened successfully.\n", device.c_str());

    // Highest refresh rate this string length allows.  KMS starts the display at
    // that rate (minimal blanking); the actual rate is lowered per-sequence to
    // min(sequenceRate, m_configuredMaxFps) in PrepData().
    m_configuredMaxFps = fb->GetMaxRefreshRate();
    m_currentFps = m_configuredMaxFps;
    LogInfo(VB_CHANNELOUT, "DPIPixels: framebuffer %dx%d, max refresh %d fps for %d channel longest string\n",
            fb->Width(), fb->Height(), m_configuredMaxFps, longestString);

    bool initOK = false;
    if (protocol == "ws2811") {
        initOK = InitializeWS281x();
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

        for (int page = 0; page < fb->PageCount(); page++) {
            fbPage = page;
            PrepData(blankData.data());
        }
        fbPage = 0;
    }

    // Configure DPI pins - they will output blank data from framebuffer
    for (const auto& pinName : m_configuredDPIPins) {
        try {
            LogDebug(VB_CHANNELOUT, "Enabling pin %s for DPI\n", pinName.c_str());
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            pin.configPin("dpi", true, "DPIPixels-" + pinName);
        } catch (const std::exception& ex) {
            LogErr(VB_CHANNELOUT, "Failed to configure %s as DPI: %s\n", pinName.c_str(), ex.what());
        }
    }

#ifdef LOGIC_ANALYZING
    // Turn on the HSync and VSync pins for logic analyzing
    const PinCapabilities& hs_pin = PinCapabilities::getPinByName("P1-5");
    hs_pin.configPin("dpi");
    const PinCapabilities& vs_pin = PinCapabilities::getPinByName("P1-3");
    vs_pin.configPin("dpi");
#endif

    // Enable runtime refresh-rate control now that pins are live.
    m_initialized = true;

    return ChannelOutput::Init(config);
}

int DPIPixelsOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "DPIPixelsOutput::Close()\n");

    // Stop runtime rate control so the blanking PrepData() calls below don't
    // trigger a modeset while we're tearing down.
    m_initialized = false;

    // Send blank WS281x data to clear latched pixels before shutdown
    if (fb && pixelStrings.size() > 0 && protocol == "ws2811") {
        std::vector<unsigned char> blankData(FPPD_MAX_CHANNELS, 0);

        for (int page = 0; page < fb->PageCount(); page++) {
            fbPage = page;
            PrepData(blankData.data());
        }
        fbPage = 0;

        fb->SyncDisplay(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Reconfigure DPI pins back to GPIO input
    for (const auto& pinName : m_configuredDPIPins) {
        try {
            const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
            pin.releasePin();
        } catch (...) {
        }
    }
    m_configuredDPIPins.clear();

    for (auto& n : m_autoCreatedModelNames) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(n);
    }
    return ChannelOutput::Close();
}

void DPIPixelsOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    PixelString* ps = NULL;
    for (int s = 0; s < pixelStrings.size(); s++) {
        ps = pixelStrings[s];
        int inCh = 0;
        int min = FPPD_MAX_CHANNELS;
        int max = 0;
        for (int p = 0; p < ps->m_outputBytes; p++) {
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
    // Match the DPI output rate to the running sequence, capped by what the
    // longest string physically allows.  A change only happens when a sequence
    // with a different frame rate starts, so the (brief) modeset is rare.
    //
    // With no sequence running, the rate this follows is whatever the last
    // sequence left behind -- nothing in core lowers or restores it -- or the
    // 20fps default at boot.  That pinned live control (pixel overlays, effects,
    // a lighting desk driving overlays) to a 50ms vblank, so a colour change
    // waited up to a full frame to reach the string on top of the output
    // thread's own period.  Running fast while idle costs nothing: SetRefreshRate
    // only shortens vertical blanking, so the pixel clock and the WS bit timing
    // are untouched and the string is simply rescanned more often.
    //
    // Only raise after a settle window, though.  A rate change is a full
    // drmModeSetCrtc that drains the pending flip and restarts the vblank
    // stream, so bouncing on the brief gaps between playlist items would glitch
    // the output every time.  Dropping back to a sequence's rate stays immediate
    // -- that direction has to track the data.
    if (m_initialized && m_configuredMaxFps > 0) {
        constexpr long long IDLE_SETTLE_US = 3000000;

        int target = (int)std::lround(GetChannelOutputRefreshRate());
        bool idle = !sequence->IsSequenceRunning();
        if (!idle) {
            m_idleSinceUS = 0;
        } else {
            long long now = GetTime();
            if (m_idleSinceUS == 0) {
                m_idleSinceUS = now;
            }
            if ((now - m_idleSinceUS) >= IDLE_SETTLE_US) {
                target = m_configuredMaxFps;
            }
        }
        if (target < 1) {
            target = 1;
        }
        if (target > m_configuredMaxFps) {
            target = m_configuredMaxFps;
        }
        if (target != m_currentFps && fb->SetRefreshRate(target)) {
            LogInfo(VB_CHANNELOUT, "DPIPixels: DPI refresh -> %d fps (%s, sequence rate %.1f, max %d)\n",
                    target, idle ? "idle" : "playing",
                    GetChannelOutputRefreshRate(), m_configuredMaxFps);
            m_currentFps = target;
        }
    }

    PixelString* ps = NULL;
    int ch = 0;
#ifdef LOG_ELAPSED_TIME
    long long startTime = 0;
    long long elapsedTimeGather = 0;
    long long elapsedTimeTranspose = 0;
    long long elapsedTimeOutput = 0;
#endif

#ifdef USE_AUTO_SYNC
    fbPage = fb->Page(true);

    if (fb->IsDirty(fbPage)) {
        LogErr(VB_CHANNELOUT, "FB Page %d is dirty, output thread isn't keeping up!\n", fbPage);
        fbPage = -1;
        return;
    }

    fb->NextPage(true);
#else
    fbPage = fb->Page();
#endif

    protoBitOnLine = 0;

    uint8_t* outputBuffers[80];
    PixelStringTester* tester = nullptr;
    if (m_testType && m_testCycle >= 0) {
        tester = PixelStringTester::getPixelStringTester(m_testType);
        tester->prepareTestData(m_testCycle, m_testPercent);
    }
    for (int x = 0; x < 80; x++) {
        if (x < pixelStrings.size()) {
            ps = pixelStrings[x];
            uint32_t newLen = 0;
            outputBuffers[x] = tester
                                   ? tester->createTestData(ps, m_testCycle, m_testPercent, channelData, newLen)
                                   : ps->prepareOutput(channelData);
        } else {
            outputBuffers[x] = nullptr;
        }
    }

    // Start at front of page but skip the first third of the WS bit
    // which is already populated and doesn't change
    protoDest = fb->BufferPage(fbPage) + (fbPixelMult * fb->BytesPerPixel());

    uint32_t dataIn[MAX_DPI_PIXEL_LATCHES][32];
    uint32_t dataOut[MAX_DPI_PIXEL_LATCHES][32];
    int lineOffset = fb->RowPadding();
    int output = 0;
    int dataSets = usingLatches ? MAX_DPI_PIXEL_LATCHES : 1;

#ifdef LOG_ELAPSED_TIME
    startTime = GetTime();
#endif

    // Write data out 4 channels (32 WS bits) at a time
    for (int y0 = 0, y1 = 1, y2 = 2, y3 = 3; y0 < longestString; y0 += 4, y1 += 4, y2 += 4, y3 += 4) {
        memset(dataIn, 0, sizeof(dataIn));

        // Storing up to 32 bits of data for up to 24 outputs (or 24 per latch)
        for (int lp = 0; lp < dataSets; lp++) {
            for (int o = 0; o < 24; o++) {
                int strNum = (lp * 24) + o;
                output = outputToStringMap[strNum];
                if ((output != -1) && (outputBuffers[output])) {
                    dataIn[lp][31 - o] =
                        (y0 >= stringLengths[strNum] ? 0 : ((uint32_t)outputBuffers[output][y0] << 24)) |
                        (y1 >= stringLengths[strNum] ? 0 : ((uint32_t)outputBuffers[output][y1] << 16)) |
                        (y2 >= stringLengths[strNum] ? 0 : ((uint32_t)outputBuffers[output][y2] << 8)) |
                        (y3 >= stringLengths[strNum] ? 0 : ((uint32_t)outputBuffers[output][y3]));
                }
            }
        }

#ifdef LOG_ELAPSED_TIME
        elapsedTimeGather += GetTime() - startTime;
        startTime = GetTime();
#endif

        for (int lp = 0; lp < dataSets; lp++) {
            TransposeBits32x32(dataOut[lp], dataIn[lp]);
        }

#ifdef LOG_ELAPSED_TIME
        elapsedTimeTranspose += GetTime() - startTime;
        startTime = GetTime();
#endif

        // Write 4 channels (32 bits) of data for 24 outputs (or 24 per latch)
        int ch = 0;
        for (int bit = 0; bit < 32; bit++) {
            if (usingLatches) {
                ch = y0 + (bit / 8);
                for (int lp = 0; lp < dataSets; lp++) {
                    if (lp < latchCount) {
                        WriteLatchedDataAtPosition(protoDest, (uint32_t)dataOut[lp][bit], latchPinMasks[lp]);
                    } else {
                        protoDest += 4 * fb->BytesPerPixel();
                    }
                }
            } else {
                WriteDataAtPosition(protoDest, (uint32_t)dataOut[0][bit]);
            }

            protoBitOnLine++;

            // Skip the static last third of this WS BIT and first third of next WS bit
            // and position at middle third of next WS bit
            protoDest += fb->BytesPerPixel() * fbPixelMult * 2;

            // When necessary, jump to middle third of first WS bit on next line
            if (protoBitOnLine >= protoBitsPerLine) {
                protoDest += lineOffset;
                protoBitOnLine = 0;
            }
        }

#ifdef LOG_ELAPSED_TIME
        elapsedTimeOutput += GetTime() - startTime;
#endif
    }

#ifdef LOG_ELAPSED_TIME
    LogInfo(VB_CHANNELOUT, "Data Gather     : %lldus\n", elapsedTimeGather);
    LogInfo(VB_CHANNELOUT, "Data Transpose  : %lldus\n", elapsedTimeTranspose);
    LogInfo(VB_CHANNELOUT, "Data Output     : %lldus\n", elapsedTimeOutput);
    LogInfo(VB_CHANNELOUT, "PrepData() Total: %lldus\n", elapsedTimeGather + elapsedTimeTranspose + elapsedTimeOutput);
#endif
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
    LogDebug(VB_CHANNELOUT, "protoDestExtra     : %d\n", protoDestExtra);
    LogDebug(VB_CHANNELOUT, "longestString      : %d (channels)\n", longestString);

    LogDebug(VB_CHANNELOUT, "Strings:\n");
    for (int i = 0; i < pixelStrings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "  String #%d\n", i);
        pixelStrings[i]->DumpConfig();
    }

    ChannelOutput::DumpConfig();
}

int DPIPixelsOutput::GetDPIPinBitPosition(std::string pinName) {
    int bitPosition = -1;

    // https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#gpio-pins
    // /dev/fb* is BGR order even though we are using RGB888
    // The FB color order is handled inside Write(Latched)DataAtPosition().  Internally, we
    // use RGB in the lower 24 bits of uint32_t.

    // clang-format off
    // Blue in RGB888                              // DPI Pin - GPIO # - Position in RGB888
    if      (pinName == "P1-7")  bitPosition =  0; // DPI_D0  - GPIO4  - 0x000001
    else if (pinName == "P1-29") bitPosition =  1; // DPI_D1  - GPIO5  - 0x000002
    else if (pinName == "P1-31") bitPosition =  2; // DPI_D2  - GPIO6  - 0x000004
    else if (pinName == "P1-26") bitPosition =  3; // DPI_D3  - GPIO7  - 0x000008
    else if (pinName == "P1-24") bitPosition =  4; // DPI_D4  - GPIO8  - 0x000010
    else if (pinName == "P1-21") bitPosition =  5; // DPI_D5  - GPIO9  - 0x000020
    else if (pinName == "P1-19") bitPosition =  6; // DPI_D6  - GPIO10 - 0x000040
    else if (pinName == "P1-23") bitPosition =  7; // DPI_D7  - GPIO11 - 0x000080
    // Green in RGB888
    else if (pinName == "P1-32") bitPosition =  8; // DPI_D8  - GPIO12 - 0x000100
    else if (pinName == "P1-33") bitPosition =  9; // DPI_D9  - GPIO13 - 0x000200
    else if (pinName == "P1-8")  bitPosition = 10; // DPI_D10 - GPIO14 - 0x000400
    else if (pinName == "P1-10") bitPosition = 11; // DPI_D11 - GPIO15 - 0x000800
    else if (pinName == "P1-36") bitPosition = 12; // DPI_D12 - GPIO16 - 0x001000
    else if (pinName == "P1-11") bitPosition = 13; // DPI_D13 - GPIO17 - 0x002000
    else if (pinName == "P1-12") bitPosition = 14; // DPI_D14 - GPIO18 - 0x004000
    else if (pinName == "P1-35") bitPosition = 15; // DPI_D15 - GPIO19 - 0x008000
    // Red in RGB888
    else if (pinName == "P1-38") bitPosition = 16; // DPI_D16 - GPIO20 - 0x010000
    else if (pinName == "P1-40") bitPosition = 17; // DPI_D17 - GPIO21 - 0x020000
    else if (pinName == "P1-15") bitPosition = 18; // DPI_D18 - GPIO22 - 0x040000
    else if (pinName == "P1-16") bitPosition = 19; // DPI_D19 - GPIO23 - 0x080000
    else if (pinName == "P1-18") bitPosition = 20; // DPI_D20 - GPIO24 - 0x100000
    else if (pinName == "P1-22") bitPosition = 21; // DPI_D21 - GPIO25 - 0x200000
    else if (pinName == "P1-37") bitPosition = 22; // DPI_D22 - GPIO26 - 0x400000
    else if (pinName == "P1-13") bitPosition = 23; // DPI_D23 - GPIO27 - 0x800000
    // clang-format on

    return bitPosition;
}

bool DPIPixelsOutput::FrameBufferIsConfigured(void) {
    if (!fb)
        return false;

    if (protocol != "ws2811")
        return false;

    std::string errStr = "";

    // The DPI overlay (config.txt: dtoverlay=vc4-kms-dpi-fpp) fixes the width at
    // 1920 and the pixel clock at 38.4MHz, which is what makes each WS bit 48 FB
    // pixels (1.25us).  Without it the bit timing is wrong.
    if (fb->Width() != 1920) {
        errStr = m_outputType + " Framebuffer width " + std::to_string(fb->Width()) +
                 " is not 1920; the DPI overlay is not configured.  A reboot may be required to load it.";
    } else if (!fb->SupportsVariableRefresh()) {
        // Non-KMS fallback (e.g. IOCTL fbdev) cannot retime the output; the
        // variable frame rate feature requires the KMS DPI device.
        errStr = m_outputType + " Framebuffer does not support variable refresh; KMS DPI output is required.";
    } else if (longestString > 4800) {
        // Hard limit from onOffMask[][4800] (1600 pixels per string).
        errStr = m_outputType + " channel count " + std::to_string(longestString) +
                 " is too high.  Max 4800 channels (1600 pixels) per string.";
    } else {
        return true;
    }

    WarningHolder::AddWarning(errStr);
    errStr += "\n";
    LogErr(VB_CHANNELOUT, errStr.c_str());
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
    int sEnd = pixelStrings.size();
    uint32_t pinsOn[MAX_DPI_PIXEL_LATCHES];

    fb->ClearAllPages();

    // 16 FB pixels per third of WS bit gives us room to turn on/off 4 sets of latches
    fbPixelMult = 16; // @ 38.4Mhz, 1920 FB width

    protoBitOnLine = 0;

    // Each WS bit is split into three chunks of fbPixelMult FB pixels
    protoBitsPerLine = fb->Width() / (3 * fbPixelMult);

    // Initialize to first FB pixel
    protoDest = fb->BufferPage(0);

    // Skip over the hsync/porch pad area
    protoDestExtra = fb->RowPadding();

#ifdef LOG_ELAPSED_TIME
    long long endTime = 0;
    long long elapsed = 0;
    long long startTime = GetTime();
#endif

    if (usingLatches) {
        uint32_t nonLatchPins = 0xFFFFFF;
        // Round up to next multiple of 4 since we will later write out data 4 channels at a time.
        int lastChannel = (longestString + 3) & ~0x03;

        for (int lp = 0; lp < MAX_DPI_PIXEL_LATCHES; lp++) {
            nonLatchPins -= latchPinMasks[lp];
        }

        memset(pinsOn, 0, sizeof(pinsOn));

        for (int y = 0; y < lastChannel; y++) {
            // Figure out which pins are allowed to be on for each latch
            for (int lp = 0; lp < latchCount; lp++) {
                pinsOn[lp] = nonLatchPins & onOffMask[lp][y];

                // Force pins off where the string length is less than the current channel
                for (int p = 0; p < 24; p++) {
                    int outputNumber = outputToStringMap[(lp * 24) + p];
                    if (y >= stringLengths[(lp * 24) + p]) {
                        pinsOn[lp] = pinsOn[lp] & ~POSITION_TO_BITMASK(p);
                    }
                }
            }

            // Populate 8 bits for each channel
            for (int b = 0; b < 8; b++) {
                // Setup FB pixels for first third of WS bit.  These will not be modified later.
                // Stays high whether WS bit is a 0 or 1.
                for (int lp = 0; lp < MAX_DPI_PIXEL_LATCHES; lp++) {
                    if (lp < latchCount)
                        WriteLatchedDataAtPosition(protoDest, pinsOn[lp], latchPinMasks[lp]);
                    else
                        protoDest += 4 * fb->BytesPerPixel();
                }

                // Setup FB pixels for middle third of WS bit
                // Set Low or High depending on whether WS bit is a 0 or 1.  Set to low initially.
                for (int lp = 0; lp < MAX_DPI_PIXEL_LATCHES; lp++) {
                    if (lp < latchCount)
                        WriteLatchedDataAtPosition(protoDest, 0x000000, latchPinMasks[lp]);
                    else
                        protoDest += 4 * fb->BytesPerPixel();
                }

                // Setup FB pixels for last third of WS bit.  These will not be modified later.
                // Stays low whether WS bit is a 0 or 1.
                for (int lp = 0; lp < MAX_DPI_PIXEL_LATCHES; lp++) {
                    if (lp < latchCount)
                        WriteLatchedDataAtPosition(protoDest, 0x000000, latchPinMasks[lp]);
                    else
                        protoDest += 4 * fb->BytesPerPixel();
                }

                protoBitOnLine++;

                if (protoBitOnLine >= protoBitsPerLine) {
                    protoDest += protoDestExtra;
                    protoBitOnLine = 0;
                }
            }
        }
    } else {
        // NOT using latches
        latchPinMask = 0x000000;

        while (y < longestString) {
            // For each WS281x bit on the scan line
            for (int x = 0; x < protoBitsPerLine; x += 8) {
                // For each bit in the WS281x pixel protocol
                for (int rp = 0; rp < 8; rp++) {
                    if (licensedOutputs) {
                        if (usingSmartReceivers) {
                            onOff = 0x000000;
                            for (int s = sStart; s < sEnd; s++) {
                                if ((bitPos[s - sStart] != -1) && (onOffMap[strPixel * pixelStrings.size() + s]))
                                    onOff |= POSITION_TO_BITMASK(bitPos[s - sStart]);
                            }
                        } else {
                            onOff = 0xFFFFFF;
                        }
                    } else {
                        onOff = 0xFFFFFF;
                    }

                    // Update FB pixels making up the first third of the WS bit.
                    // These FB pixel will never be modified again.
                    for (int i = 0; i < fbPixelMult; i++) {
                        *(protoDest++) = (onOff >> 16);
                        *(protoDest++) = (onOff >> 8);
                        *(protoDest++) = (onOff);
                    }

                    // Skip over the last two-thirds of the current WS bit.
                    // The middle third will get updated by in PrepData() later
                    // and the last two-thirds is always 0x000000 for the low.
                    protoDest += fb->BytesPerPixel() * fbPixelMult * 2;
                }

                y++;
                strPixel++;
            }

            // Skips any row padding
            protoDest += protoDestExtra;
        }

        // Inside PrepData(), we'll skip an extra two-thirds WS BIT pixels due to the way it works
        protoDestExtra += fb->BytesPerPixel() * fbPixelMult * 2;

        // int perLine = fb->BytesPerPixel() * fbPixelMult * 3;
        // int header = fb->BytesPerPixel() * sBeginEndSize;
        // int numLines = 76 * 8;
        // HexDump("fb data:", fb->BufferPage(0) + header, numLines * perLine, VB_CHANNELOUT, perLine);
    }

#ifdef LOG_ELAPSED_TIME
    endTime = GetTime();
    elapsed = endTime - startTime;
    LogInfo(VB_CHANNELOUT, "InitializeWS2811 Elapsed: %lld\n", elapsed);
#endif

    for (int p = 1; p < fb->PageCount(); p++) {
        // Copy first page to rest of pages
        memcpy(fb->BufferPage(p), fb->BufferPage(0), fb->PageSize());
    }

    fb->SyncDisplay(true);
    fb->NextPage();

    return true;
}
