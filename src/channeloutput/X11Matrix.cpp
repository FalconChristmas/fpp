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

#include <X11/Xutil.h>

#include "../log.h"

#include "X11Matrix.h"
#include "serialutil.h"

#include "Plugin.h"
class X11MatrixPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    X11MatrixPlugin() :
        FPPPlugins::Plugin("X11Matrix") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new X11MatrixOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new X11MatrixPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
X11MatrixOutput::X11MatrixOutput(unsigned int startChannel,
                                 unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
    m_width(0),
    m_height(0),
    m_scale(10),
    m_scaleWidth(0),
    m_scaleHeight(0),
    m_imageData(NULL) {
    LogDebug(VB_CHANNELOUT, "X11MatrixOutput::X11MatrixOutput(%u, %u)\n",
             startChannel, channelCount);

    XInitThreads();

    m_useDoubleBuffer = 1;
}

/*
 *
 */
X11MatrixOutput::~X11MatrixOutput() {
    LogDebug(VB_CHANNELOUT, "X11MatrixOutput::~X11MatrixOutput()\n");
}

/*
 *
 */
int X11MatrixOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "X11MatrixOutput::Init(JSON)\n");

    m_width = config["width"].asInt();
    m_height = config["height"].asInt();
    m_scale = config["scale"].asInt();

    if (config.isMember("title"))
        m_title = config["title"].asString();
    else
        m_title = "X11Matrix";

    if (!m_scale)
        m_scale = 10;

    m_scaleWidth = m_scale * m_width;
    m_scaleHeight = m_scale * m_height;
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

/*
 *
 */
int X11MatrixOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "X11MatrixOutput::Close()\n");

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

/*
 *
 */
int X11MatrixOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "X11MatrixOutput::RawSendData(%p)\n", channelData);

    // Draw on X11 Window here
    char* c = (char*)channelData;
    char* d = m_imageData;
    char r = 0;
    char g = 0;
    char b = 0;
    char* row = NULL;
    int stride = m_scaleWidth * 4;
    for (int y = 0; y < m_height; y++) {
        row = d;

        for (int x = 0; x < m_width; x++) {
            r = *(c++);
            g = *(c++);
            b = *(c++);

            for (int s = 0; s < m_scale; s++) {
                *(d++) = b;
                *(d++) = g;
                *(d++) = r;
                *(d++) = 0;
            }
        }

        for (int s = 1; s < m_scale; s++) {
            memcpy(row + stride, row, stride);
            row += stride;
            d += stride;
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
void X11MatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
void X11MatrixOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "X11MatrixOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Width         : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    Height        : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "    Scale         : %d\n", m_scale);
    LogDebug(VB_CHANNELOUT, "    Scaled Width  : %d\n", m_scaleWidth);
    LogDebug(VB_CHANNELOUT, "    Scaled Height : %d\n", m_scaleHeight);

    ThreadedChannelOutput::DumpConfig();
}
