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

#include <string>

#include "Matrix.h"
#include "PanelMatrix.h"

#include "led-matrix.h"

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;

#include "ChannelOutput.h"

class RGBMatrixOutput : public ChannelOutput {
public:
    RGBMatrixOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~RGBMatrixOutput();

    virtual std::string GetOutputType() const override {
        return "Pi Panels";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const override { return true; }

private:
    FrameCanvas* m_canvas = nullptr;
    RGBMatrix* m_rgbmatrix = nullptr;
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

    Matrix* m_matrix = nullptr;
    PanelMatrix* m_panelMatrix = nullptr;

    uint8_t m_gammaCurve[256];
};
