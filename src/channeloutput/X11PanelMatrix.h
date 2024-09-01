#pragma once
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

#include <X11/Xlib.h>

#include <string>

#include "Matrix.h"
#include "PanelMatrix.h"

#include "ThreadedChannelOutput.h"

class X11PanelMatrixOutput : public ThreadedChannelOutput {
public:
    X11PanelMatrixOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~X11PanelMatrixOutput();

    virtual std::string GetOutputType() const {
        return "X11PanelMatrix";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    std::string m_layout;
    std::string m_colorOrder;

    int m_panelWidth;
    int m_panelHeight;
    int m_panels;
    int m_width;
    int m_height;
    int m_rows;
    int m_outputs;
    int m_longestChain;
    int m_invertedData;

    Matrix* m_matrix;
    PanelMatrix* m_panelMatrix;

    uint8_t m_gammaCurve[256];

    int m_scale;
    int m_scaleWidth;
    int m_scaleHeight;

    char* m_imageData;

    Display* m_display;
    int m_screen;
    Window m_window;
    GC m_gc;
    Pixmap m_pixmap;
    XImage* m_image;

    std::string m_title;
};
