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

#include <cmath>

#include <X11/Xutil.h>

#include "../log.h"

#include "GammaLUT.h"
#include "X11PanelMatrix.h"

#include "Plugin.h"
class X11PanelMatrixPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    X11PanelMatrixPlugin() :
        FPPPlugins::Plugin("X11PanelMatrix") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new X11PanelMatrixOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new X11PanelMatrixPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
X11PanelMatrixOutput::X11PanelMatrixOutput(unsigned int startChannel,
                                           unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
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
    LogDebug(VB_CHANNELOUT, "X11PanelMatrixOutput::X11PanelMatrixOutput(%u, %u)\n",
             startChannel, channelCount);

    XInitThreads();

    m_useDoubleBuffer = 1;
}

/*
 *
 */
X11PanelMatrixOutput::~X11PanelMatrixOutput() {
    LogDebug(VB_CHANNELOUT, "X11PanelMatrixOutput::~X11PanelMatrixOutput()\n");

    delete m_matrix;
    delete m_panelMatrix;
}

/*
 *
 */
int X11PanelMatrixOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "X11PanelMatrixOutput::Init(JSON)\n");

    m_panelWidth = config["panelWidth"].asInt();
    m_panelHeight = config["panelHeight"].asInt();

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

    m_rows = m_panelHeight;

    m_width = m_panelMatrix->Width();
    m_height = m_panelMatrix->Height();

    m_channelCount = m_width * m_height * 3;

    int colorDepth = 8;
    if (config.isMember("panelColorDepth")) {
        colorDepth = config["panelColorDepth"].asInt();
    }
    if (colorDepth > 11 || colorDepth < 6) {
        colorDepth = 11;
    }

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

    if (config.isMember("scale"))
        m_scale = config["scale"].asInt();
    else
        m_scale = 10;

    if (config.isMember("title"))
        m_title = config["title"].asString();
    else
        m_title = "X11PanelMatrix";

    m_scaleWidth = m_width * m_scale;
    m_scaleHeight = m_height * m_scale;
    m_imageData = (char*)calloc(m_scaleWidth * m_scaleHeight * 4, 1);

    // Initialize X11 Window here
    const char* dsp = getenv("DISPLAY");
    if (dsp == nullptr) {
        dsp = ":0";
    }
    m_display = XOpenDisplay(dsp);
    if (!m_display) {
        LogErr(VB_CHANNELOUT, "Unable to connect to X Server: %s\n", dsp);
        return 0;
    }

    m_screen = DefaultScreen(m_display);

    m_image = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
                           m_imageData, m_scaleWidth, m_scaleHeight, 32, m_scaleWidth * 4);

    int win_x = 100;
    int win_y = 100;
    XSetWindowAttributes attributes;

    attributes.background_pixel = BlackPixel(m_display, m_screen);

    XGCValues values;

    m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display),
                             m_scaleWidth, m_scaleHeight, 24);

    m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
    int32_t tgc = reinterpret_cast<uintptr_t>(m_gc);
    if (tgc < 0) {
        LogErr(VB_CHANNELOUT, "Unable to create GC\n");
        return 0;
    }

    m_window = XCreateWindow(
        m_display, RootWindow(m_display, m_screen), win_x, win_y,
        m_scaleWidth, m_scaleHeight, 5, 24, InputOutput,
        DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

    XMapWindow(m_display, m_window);

    XStoreName(m_display, m_window, m_title.c_str());
    XSetIconName(m_display, m_window, m_title.c_str());

    XFlush(m_display);

    return ThreadedChannelOutput::Init(config);
}

void X11PanelMatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int X11PanelMatrixOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "X11PanelMatrixOutput::Close()\n");

    // Close X11 Window here
    if (m_display) {
        XLockDisplay(m_display);
        XDestroyWindow(m_display, m_window);
        XFreePixmap(m_display, m_pixmap);
        XFreeGC(m_display, m_gc);
        XDestroyImage(m_image);
        XUnlockDisplay(m_display);
        XCloseDisplay(m_display);
    }
    m_display = nullptr;

    return ThreadedChannelOutput::Close();
}

// The submatrix overlay has to see the whole sequence buffer: a submatrix
// names its source and its enable flag by absolute channel, and those live
// outside this output's own range.  RawSendData only ever gets the double
// buffered copy of this output's channels, so the overlay belongs here, on the
// absolute buffer, which is where the other Matrix users do it too.
void X11PanelMatrixOutput::PrepData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "X11PanelMatrixOutput::PrepData(%p)\n", channelData);
    m_matrix->OverlaySubMatrices(channelData);
}

/*
 *
 */
int X11PanelMatrixOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "X11PanelMatrixOutput::RawSendData(%p)\n",
              channelData);

    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char* c;
    unsigned int stride = m_scaleWidth * 4;

    // channelData is this output's channels only, already based at its start
    // channel, and the panel pixel maps are relative to that same base

    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();

        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int chain = m_panelMatrix->m_panels[panel].chain;
            int py = m_panelMatrix->m_panels[panel].yOffset;
            int px = m_panelMatrix->m_panels[panel].xOffset;
            for (int y = 0; y < m_panelHeight; y++) {
                int yw = y * m_panelWidth;
                for (int x = 0; x < m_panelWidth; x++) {
                    r = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(yw + x) * 3]]];
                    g = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(yw + x) * 3 + 1]]];
                    b = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(yw + x) * 3 + 2]]];

                    c = (unsigned char*)m_imageData + ((py + y) * m_scale * stride) + ((px + x) * 4 * m_scale);

                    for (unsigned int t = 0; t < m_scale; t++) {
                        for (unsigned int s = 0; s < m_scale; s++) {
                            *(c++) = b;
                            *(c++) = g;
                            *(c++) = r;
                            c++;
                        }
                        c += stride - (m_scale * 4);
                    }
                }
            }
        }
    }

    XLockDisplay(m_display);

    XPutImage(m_display, m_window, m_gc, m_image, 0, 0, 0, 0, m_scaleWidth, m_scaleHeight);

    XSync(m_display, True);
    XFlush(m_display);

    XUnlockDisplay(m_display);

    return m_channelCount;
}

/*
 *
 */
void X11PanelMatrixOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "X11PanelMatrixOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    Panels        : %d\n", m_panels);
    LogDebug(VB_CHANNELOUT, "    Width         : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    Height        : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "    Scale         : %d\n", m_scale);
    LogDebug(VB_CHANNELOUT, "    Scaled Width  : %d\n", m_scaleWidth);
    LogDebug(VB_CHANNELOUT, "    Scaled Height : %d\n", m_scaleHeight);
}
