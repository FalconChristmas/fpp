/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include "fpp-json.h"
#include "common.h"
#include "Sequence.h"

#include <cmath>
#include <unistd.h>

#include "../Warnings.h"
#include "../log.h"

#include "GammaLUT.h"
#include "RGBMatrix.h"
#include "overlays/PixelOverlay.h"

#include "Plugin.h"
class RGBMatrixPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    RGBMatrixPlugin() :
        FPPPlugins::Plugin("RGBMatrix") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new RGBMatrixOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new RGBMatrixPlugin();
}
}

// The library's own pin-mapping table.  Framebuffer's constructor abort()s when
// a mapping is asked for more parallel chains than it has pins for, so the
// limit has to be checked against the same table before handing it options --
// a hand-kept copy here had already drifted (it said regular-pi1 had 2).
#include "../lib/hardware-mapping.h"

// Mirrors Framebuffer::InitHardwareMapping(): 0 for an unknown mapping name,
// otherwise the number of parallel chains the mapping has data pins for.
static int MaxParallelChains(const char* name) {
    if (name == nullptr || *name == '\0') {
        name = "regular";
    }
    for (const HardwareMapping* h = matrix_hardware_mappings; h->name; ++h) {
        if (strcasecmp(h->name, name) != 0) {
            continue;
        }
        if (h->max_parallel_chains) {
            return h->max_parallel_chains;
        }
        int n = 0;
        n += (h->p0_r1 | h->p0_g1 | h->p0_r2 | h->p0_g2) ? 1 : 0;
        n += (h->p1_r1 | h->p1_g1 | h->p1_r2 | h->p1_g2) ? 1 : 0;
        n += (h->p2_r1 | h->p2_g1 | h->p2_r2 | h->p2_g2) ? 1 : 0;
        n += (h->p3_r1 | h->p3_g1 | h->p3_r2 | h->p3_g2) ? 1 : 0;
        n += (h->p4_r1 | h->p4_g1 | h->p4_r2 | h->p4_g2) ? 1 : 0;
        n += (h->p5_r1 | h->p5_g1 | h->p5_r2 | h->p5_g2) ? 1 : 0;
        return n;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
RGBMatrixOutput::RGBMatrixOutput(unsigned int startChannel,
                                 unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_canvas(NULL),
    m_rgbmatrix(NULL),
    m_colorOrder("RGB"),
    m_panelWidth(32),
    m_panelHeight(16),
    m_panels(0),
    m_width(0),
    m_height(0),
    m_rows(0),
    m_outputs(0),
    m_longestChain(0),
    m_invertedData(0) {
    LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::RGBMatrixOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
RGBMatrixOutput::~RGBMatrixOutput() {
    LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::~RGBMatrixOutput()\n");

    if (m_rgbmatrix)
        delete m_rgbmatrix;
    if (m_matrix)
        delete m_matrix;
    if (m_panelMatrix)
        delete m_panelMatrix;
}

/*
 *
 */
int RGBMatrixOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Init(JSON)\n");
    std::string model = GetFileContents("/proc/device-tree/model");
    if (startsWith(model, "Raspberry Pi 5") ||
        startsWith(model, "Raspberry Pi Compute Module 5")) {
        LogErr(VB_CHANNELOUT, "RGBMatrix does work on Raspberry Pi 5\n");
        WarningHolder::AddWarning(50, "LED panel output: not supported on the Raspberry Pi 5 / CM5");
        return 0;
    }
    if (m_channelCount > FPPD_MAX_CHANNELS) {
        LogErr(VB_CHANNELOUT, "RGBMatrixOutput::Init: channelCount %u exceeds maximum of %u\n",
               m_channelCount, FPPD_MAX_CHANNELS);
        WarningHolder::AddWarning(50, "LED panel output: channel count exceeds the maximum supported");
        return 0;
    }

    m_panelWidth = config["panelWidth"].asInt();
    m_panelHeight = config["panelHeight"].asInt();

    std::string modules = GetFileContents("/proc/modules");
    if (modules.contains("snd_bcm2835")) {
        LogInfo(VB_CHANNELOUT, "snd_bcm2835 is loaded, attempting to unload.");
        system("rmmod snd_bcm2835");
        system("modprobe snd_dummy");
    }
    modules = GetFileContents("/proc/modules");
    if (modules.contains("snd_bcm2835")) {
        LogWarn(VB_CHANNELOUT, "snd_bcm2835 is stil loaded. Cannot proceed.");
        std::string nvresults;
        urlPut("http://127.0.0.1/api/settings/rebootFlag", "1", nvresults);
        std::string errStr = "RGBMatrix cannot run with snd_bcm2835 enabled.   Please reboot.";
        WarningHolder::AddWarning(50, errStr);
        errStr += "\n";
        LogErr(VB_CHANNELOUT, errStr.c_str());
        return false;
    }

    if (!m_panelWidth)
        m_panelWidth = 32;

    if (!m_panelHeight)
        m_panelHeight = 16;

    m_invertedData = config["invertedData"].asInt();
    m_colorOrder = config["colorOrder"].asString();

    m_panelMatrix =
        new PanelMatrix(m_panelWidth, m_panelHeight, m_invertedData);

    if (!m_panelMatrix) {
        LogErr(VB_CHANNELOUT, "Unable to create PanelMatrix\n");
        return 0;
    }

    for (int i = 0; i < config["panels"].size(); i++) {
        Json::Value p = config["panels"][i];
        char orientation = 'N';
        std::string o = p["orientation"].asString();

        if (!o.empty())
            orientation = o[0];

        if (p["colorOrder"].asString() == "")
            p["colorOrder"] = m_colorOrder;

        m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
                                p["panelNumber"].asInt(), orientation,
                                p["xOffset"].asInt(), p["yOffset"].asInt(),
                                ColorOrderFromString(p["colorOrder"].asString()));

        if (p["outputNumber"].asInt() > m_outputs)
            m_outputs = p["outputNumber"].asInt();

        if (p["panelNumber"].asInt() > m_longestChain)
            m_longestChain = p["panelNumber"].asInt();
    }

    // Both of these are 0-based, so bump them up by 1 for comparisons
    m_outputs++;
    m_longestChain++;

    m_panels = m_panelMatrix->PanelCount();

    int gpioSlowdown = 1;
    if (config.isMember("gpioSlowdown"))
        gpioSlowdown = config["gpioSlowdown"].asInt();

    m_rows = m_panelHeight;

    m_width = m_panelMatrix->Width();
    m_height = m_panelMatrix->Height();

    m_channelCount = m_width * m_height * 3;

    RGBMatrix::Options options;
    rgb_matrix::RuntimeOptions runtimeOptions;

    runtimeOptions.gpio_slowdown = gpioSlowdown;
    runtimeOptions.daemon = 0;
    runtimeOptions.drop_privileges = 0;
    runtimeOptions.do_gpio_init = true;

    if (config["wiringPinout"].asString() != "")
        options.hardware_mapping = strdup(config["wiringPinout"].asString().c_str());

    options.chain_length = m_longestChain;
    options.parallel = m_outputs;
    options.rows = m_panelHeight;
    options.cols = m_panelWidth;

    if (config.isMember("brightness"))
        options.brightness = config["brightness"].asInt();
    else
        options.brightness = 100;

    int colorDepth = 8;
    if (config.isMember("panelColorDepth")) {
        colorDepth = config["panelColorDepth"].asInt();
    }
    if (colorDepth > 11 || colorDepth < 6) {
        colorDepth = 11;
    }
    options.pwm_bits = colorDepth;

    LogDebug(VB_CHANNELOUT, "  chain: %d    parallel: %d   rows: %d     cols: %d    brightness: %d   colordepth: %d\n",
             options.chain_length, options.parallel, options.rows, options.cols,
             options.brightness, options.pwm_bits);

    if (config.isMember("cpuPWM")) {
        options.disable_hardware_pulsing = config["cpuPWM"].asBool();
        if (options.disable_hardware_pulsing) {
            LogDebug(VB_CHANNELOUT, "Disabling use of Hardware PWM for OE pin\n");
        }
    }
    if (config.isMember("panelInterleave")) {
        options.multiplexing = std::atoi(config["panelInterleave"].asString().c_str());

        int panelScan = config["panelScan"].asInt();
        if (panelScan == 0) {
            // 1/8 scan by default
            panelScan = m_panelHeight / 2;
        }
        if (panelScan == (m_panelHeight / 2)) {
            options.multiplexing = 0;
        }
    }
    /* mortification77

        panelRowAddressType Definitions

        0 = default
        1 = AB-addressed panels
        2 = direct row select
        3 = ABC-addressed panels
        4 = ABC Shift + DE direct (Default: 0).

    */

    if (config.isMember("panelRowAddressType")) {
        options.row_address_type = config["panelRowAddressType"].asInt();
    }

    if (config.isMember("panelType")) {
        switch (config["panelType"].asInt()) {
        case 1:
            options.panel_type = "FM6126A";
            break;
        case 2:
            options.panel_type = "FM6127";
            break;
        default:
            break;
        }
    }

    // parallel is the number of outputs (chains) in use; chain_length is the
    // panels on each and is not limited by the pinout.
    int maxParallel = MaxParallelChains(options.hardware_mapping);
    if (maxParallel == 0) {
        LogErr(VB_CHANNELOUT, "Unknown LED panel wiring pinout '%s'\n", options.hardware_mapping);
        return 0;
    }
    if (options.parallel > maxParallel) {
        LogErr(VB_CHANNELOUT, "The %s pinout supports %d output%s, but panels are assigned to %d\n",
               options.hardware_mapping, maxParallel, maxParallel > 1 ? "s" : "", options.parallel);
        return 0;
    }

    m_rgbmatrix = RGBMatrix::CreateFromOptions(options, runtimeOptions);
    if (!m_rgbmatrix) {
        LogErr(VB_CHANNELOUT, "Unable to create RGBMatrix instance\n");
        WarningHolder::AddWarning(50, "LED panel output: unable to create RGBMatrix instance from options");
        return 0;
    }

    m_canvas = m_rgbmatrix->CreateFrameCanvas();
    FrameCanvas* c2 = m_rgbmatrix->CreateFrameCanvas();
    m_rgbmatrix->SwapOnVSync(c2);

    m_matrix = new Matrix(m_startChannel, m_width, m_height);

    if (config.isMember("subMatrices")) {
        for (int i = 0; i < config["subMatrices"].size(); i++) {
            Json::Value sm = config["subMatrices"][i];

            m_matrix->AddSubMatrix(
                sm["enabled"].asInt(),
                sm["startChannel"].asInt() - 1,
                sm["width"].asInt(),
                sm["height"].asInt(),
                sm["xOffset"].asInt(),
                sm["yOffset"].asInt());
        }
    }

    GammaLUT::Build(m_gammaCurve, GammaLUT::ParseConfig(config, 2.2f));
    if (PixelOverlayManager::INSTANCE.isAutoCreatePixelOverlayModels()) {
        std::string dd = "LED Panels";
        if (config.isMember("LEDPanelMatrixName") && !config["LEDPanelMatrixName"].asString().empty()) {
            dd = config["LEDPanelMatrixName"].asString();
        }
        if (config.isMember("description")) {
            dd = config["description"].asString();
        }
        std::string desc = dd;
        int count = 0;
        while (PixelOverlayManager::INSTANCE.getModel(desc) != nullptr) {
            count++;
            desc = dd + "-" + std::to_string(count);
        }
        PixelOverlayManager::INSTANCE.addAutoOverlayModel(desc,
                                                          m_startChannel, m_channelCount, 3,
                                                          "H", m_invertedData ? "BL" : "TL",
                                                          m_height, 1);
        m_autoCreatedModelName = desc;
    }
    return ChannelOutput::Init(config);
}
void RGBMatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int RGBMatrixOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Close()\n");

    delete m_rgbmatrix;
    m_rgbmatrix = nullptr;
    m_canvas = nullptr;

    if (!m_autoCreatedModelName.empty()) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(m_autoCreatedModelName);
    }

    return ChannelOutput::Close();
}

void RGBMatrixOutput::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];

            m_panelMatrix->m_panels[panel].drawTestPattern(channelData + m_startChannel, cycleNum, percentOfCycle, testType);
        }
    }
}

/*
 *
 */
void RGBMatrixOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "RGBMatrixOutput::PrepData(%p)\n",
              channelData);
    m_matrix->OverlaySubMatrices(channelData);

    unsigned char r;
    unsigned char g;
    unsigned char b;

    channelData += m_startChannel;

    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();

        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];

            int chain = (m_longestChain - 1) - m_panelMatrix->m_panels[panel].chain;
            for (int y = 0; y < m_panelHeight; y++) {
                int px = chain * m_panelWidth;
                for (int x = 0; x < m_panelWidth; x++) {
                    r = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3]]];
                    g = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3 + 1]]];
                    b = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3 + 2]]];

                    m_canvas->SetPixel(px, y + (output * m_panelHeight), r, g, b);

                    px++;
                }
            }
        }
    }
}

/*
 *
 */
int RGBMatrixOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "RGBMatrixOutput::RawSendData(%p)\n",
              channelData);

    m_canvas = m_rgbmatrix->SwapOnVSync(m_canvas);
    return m_channelCount;
}

/*
 *
 */
void RGBMatrixOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    panels : %d\n", m_panels);
    LogDebug(VB_CHANNELOUT, "    width  : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    height : %d\n", m_height);
}
