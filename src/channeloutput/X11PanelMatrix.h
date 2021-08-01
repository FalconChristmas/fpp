#pragma once
/*
 *   X11 LED Panel Matrix simulator for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2020 the Falcon Player Developers
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

#include <X11/Xlib.h>

#include <string>

#include "Matrix.h"
#include "PanelMatrix.h"

#include "ThreadedChannelOutputBase.h"

class X11PanelMatrixOutput : public ThreadedChannelOutputBase {
public:
    X11PanelMatrixOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~X11PanelMatrixOutput();

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
