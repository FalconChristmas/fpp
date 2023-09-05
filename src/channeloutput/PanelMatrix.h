#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <string>
#include <vector>

#include "ColorOrder.h"

#define MAX_MATRIX_OUTPUTS 24
#define MAX_PANELS_PER_OUTPUT 24
#define MAX_MATRIX_PANELS (MAX_MATRIX_OUTPUTS * MAX_PANELS_PER_OUTPUT)

class LEDPanel {
public:    
    int output;
    int chain;
    int width;
    int height;
    char orientation;
    int xOffset;
    int yOffset;

    FPPColorOrder colorOrder;

    std::vector<int> pixelMap;

    void drawNumber(int v, int x, int y, unsigned char* channelData);
    void drawTestPattern(unsigned char* channelData, int cycleNum, int testType);
};

class PanelMatrix {
public:
    PanelMatrix(int panelWidth, int panelHeight, int invertedData = 0);
    ~PanelMatrix();

    int AddPanel(int output, int chain, char orientation,
                 int xOffset, int yOffset, FPPColorOrder colorOrder = FPPColorOrder::kColorOrderRGB);

    int ConfigurePanels(std::string layout);

    int Width(void) { return m_width; }
    int Height(void) { return m_height; }
    int PanelCount(void) { return m_panelCount; }

    // Map of output channels to full matrix channels
    std::vector<int> m_outputPixelMap[MAX_MATRIX_OUTPUTS];

    // Unsorted list of panels on a output
    std::vector<int> m_outputPanels[MAX_MATRIX_OUTPUTS];
    LEDPanel m_panels[MAX_MATRIX_PANELS];

private:
    int AddPanel(std::string config);

    int CalculateMaps(void);

    int m_width;
    int m_height;
    int m_outputCount;
    int m_pixelCount;
    int m_panelCount;
    int m_panelWidth;
    int m_panelHeight;
    int m_invertedData;
};
