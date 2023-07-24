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

#include "fpp-pch.h"

#include "../common.h"
#include "../log.h"

#include "PanelMatrix.h"

PanelMatrix::PanelMatrix(int panelWidth, int panelHeight, int invertedData) :
    m_width(0),
    m_height(0),
    m_outputCount(0),
    m_pixelCount(0),
    m_panelCount(0),
    m_panelWidth(panelWidth),
    m_panelHeight(panelHeight),
    m_invertedData(invertedData) {
}

PanelMatrix::~PanelMatrix() {
}

int PanelMatrix::ConfigurePanels(std::string layout) {
    int result = 1;

    LogDebug(VB_CHANNELOUT, "Parsing Config: '%s'\n", layout.c_str());

    std::vector<std::string> panels = split(layout, '|');

    for (int i = 0; i < panels.size(); i++) {
        result &= AddPanel(panels[i]);
    }

    if (result)
        CalculateMaps();

    return result;
}

int PanelMatrix::AddPanel(std::string config) {
    int output = 0;
    int chain = 0;
    int xOffset = 0;
    int yOffset = 0;
    char orientation = 'N';

    std::vector<std::string> elems = split(config, ':');

    if (elems.size() == 5) {
        output = atoi(elems[0].c_str());
        chain = atoi(elems[1].c_str());
        xOffset = atoi(elems[3].c_str());
        yOffset = atoi(elems[4].c_str());

        if (elems[2].size())
            orientation = elems[2].c_str()[0];
    } else {
        LogErr(VB_CHANNELOUT, "Invalid Panel Config: '%s'\n", config.c_str());
        return 0;
    }

    return AddPanel(output, chain, orientation, xOffset, yOffset);
}

int PanelMatrix::AddPanel(int output, int chain, char orientation,
                          int xOffset, int yOffset, FPPColorOrder colorOrder) {
    int pWidth = m_panelWidth;
    int pHeight = m_panelHeight;

    // Orientation, Left, Right, Normal, Upside down
    if ((orientation == 'L') || (orientation == 'R')) {
        pWidth = m_panelHeight;
        pHeight = m_panelWidth;
    } else if ((orientation != 'N') && (orientation != 'U')) {
        LogErr(VB_CHANNELOUT, "Invalid orientation '%c'\n",
               orientation);
        return 0;
    }

    if ((xOffset + pWidth) > m_width) {
        m_width = xOffset + pWidth;
        m_pixelCount = m_width * m_height;
    }

    if ((yOffset + pHeight) > m_height) {
        m_height = yOffset + pHeight;
        m_pixelCount = m_width * m_height;
    }

    if (output > m_outputCount)
        m_outputCount = output;

    if ((m_panelCount < MAX_MATRIX_PANELS) &&
        (m_outputPanels[output].size() < MAX_PANELS_PER_OUTPUT)) {
        m_panels[m_panelCount].output = output;
        m_panels[m_panelCount].chain = chain;
        m_panels[m_panelCount].width = pWidth;
        m_panels[m_panelCount].height = pHeight;
        m_panels[m_panelCount].orientation = orientation;
        m_panels[m_panelCount].xOffset = xOffset;
        m_panels[m_panelCount].yOffset = yOffset;
        m_panels[m_panelCount].colorOrder = colorOrder;

        m_panels[m_panelCount].pixelMap.resize(pWidth & pHeight);

        m_outputPanels[output].push_back(m_panelCount);

        LogDebug(VB_CHANNELOUT, "Added Panel %d of size %dx%d at %dx%d facing '%c'.  Color Order: %s (%d).  Matrix size now: %dx%d. Panel Count: %d, Output: %d, Chain: %d\n",
                 m_panelCount, pWidth, pHeight, xOffset, yOffset, orientation, ColorOrderToString(colorOrder).c_str(), (int)colorOrder, m_width, m_height, m_panelCount, output, chain);
        m_panelCount++;
    }

    CalculateMaps();

    return 1;
}

int PanelMatrix::CalculateMaps(void) {
    LogDebug(VB_CHANNELOUT, "PanelMatrix::CalculateMaps(), %d panels\n",
             m_panelCount);

    for (int panel = 0; panel < m_panelCount; panel++) {
        int mOffset = 0;
        int pOffset = 0;
        int xOffset = m_panels[panel].xOffset;
        int yOffset = m_panels[panel].yOffset;
        int pWidth = m_panels[panel].width;
        int pHeight = m_panels[panel].height;

        FPPColorOrder colorOrder = m_panels[panel].colorOrder;

        // LogDebug(VB_CHANNELOUT, "P: %d, O: %c, Pos: %dx%d, w: %d, h: %d\n", panel, m_panels[panel].orientation, xOffset, yOffset, pWidth, pHeight);
        m_panels[panel].pixelMap.resize(pWidth * pHeight * 3);

        for (int y = 0; y < pHeight; y++) {
            for (int x = 0; x < pWidth; x++) {
                if (m_invertedData)
                    mOffset = (((m_height - (yOffset + y) - 1) * m_width) + (xOffset + x));
                else
                    mOffset = (((yOffset + y) * m_width) + (xOffset + x));

                switch (m_panels[panel].orientation) {
                case 'N':
                    pOffset = (y * pWidth) + x;
                    break;
                case 'U':
                    pOffset = (pWidth * pHeight) - ((y * pWidth) + x) - 1;
                    break;
                case 'L':
                    pOffset = (x * pHeight) + (pHeight - y - 1);
                    break;
                case 'R':
                    pOffset = ((pWidth - x - 1) * pHeight) + y;
                    break;
                }

                mOffset *= 3;
                pOffset *= 3;

                if (colorOrder == FPPColorOrder::kColorOrderRGB) {
                    m_panels[panel].pixelMap[pOffset] = mOffset;
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset + 2;
                } else if (colorOrder == FPPColorOrder::kColorOrderRBG) {
                    m_panels[panel].pixelMap[pOffset] = mOffset;
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset + 2;
                } else if (colorOrder == FPPColorOrder::kColorOrderGRB) {
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset;
                    m_panels[panel].pixelMap[pOffset] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset + 2;
                } else if (colorOrder == FPPColorOrder::kColorOrderGBR) {
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset;
                    m_panels[panel].pixelMap[pOffset] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset + 2;
                } else if (colorOrder == FPPColorOrder::kColorOrderBRG) {
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset;
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset] = mOffset + 2;
                } else if (colorOrder == FPPColorOrder::kColorOrderBGR) {
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset;
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset] = mOffset + 2;
                } else // Assume RGB if somehow invalid
                {
                    m_panels[panel].pixelMap[pOffset] = mOffset;
                    m_panels[panel].pixelMap[pOffset + 1] = mOffset + 1;
                    m_panels[panel].pixelMap[pOffset + 2] = mOffset + 2;
                }
            }
        }
    }

    return 1;
}

void LEDPanel::drawTestPattern(unsigned char* channelData, int cycleNum, int testType) {
    unsigned char clr[3];
    switch (cycleNum % 3) {
    case 0:
        clr[0] = clr[2] = 0;
        clr[1] = 255;
        break;
    case 1:
        clr[2] = clr[1] = 0;
        clr[0] = 255;
        break;
    default:
        clr[0] = clr[1] = 0;
        clr[2] = 255;
        break;
    }

    int actualHeight = height;
    int actualWidth = width;

    if ((orientation == 'L') || (orientation == 'R')) {
        actualHeight = width;
        actualWidth = height;
    }

    for (int y = 0; y < (actualHeight / 2); y++) {
        int yw1 = y * actualWidth * 3;
        int yw2 = (y + (actualHeight / 2)) * actualWidth * 3;

        for (int x = 0; x < actualWidth; ++x) {
            int r = 0, g = 0, b = 0;
            int r2 = 0, g2 = 0, b2 = 0;
            // Left Column
            if (x == 0) {
                r = r2 = clr[0];
                g = g2 = clr[1];
                b = b2 = clr[2];
            }
            // Right Column
            if (x == (actualWidth - 1)) {
                r = r2 = clr[1];
                g = g2 = clr[2];
                b = b2 = clr[0];
            }
            // Top Row (for yw1)
            if (y == 0) {
                r = clr[2];
                g = clr[0];
                b = clr[1];
            }
            // Bottom Row (for yw2)
            if (y == (actualHeight / 2 - 1)) {
                r2 = clr[2];
                g2 = clr[0];
                b2 = clr[1];
            }
            // Diagonal Top-Left to Middle
            if (x == (2 * y) || x == (2 * y + 1)) {
                r = g = 255;
            }
            // Diagonal Bottom-Left to Middle (part1)
            if (x == (2 * (actualHeight / 2 - 1 - y))) {
                b2 = g2 = 255;
            }
            // Diagonal Bottom-Left to Middle (part2)
            if (x == (2 * (actualHeight / 2 - 1 - y) + 1)) {
                b2 = g2 = 255;
            }
            // Square in middle
            if (x >= (actualWidth / 2 - 2) && x <= (actualWidth / 2 + 1)) {
                if (y <= 1) {
                    r2 = g2 = b2 = 255;
                } else if (y >= (actualHeight / 2 - 2)) {
                    r = g = b = 255;
                }
            }

            channelData[pixelMap[yw1 + x * 3]] = r;
            channelData[pixelMap[yw1 + x * 3 + 1]] = g;
            channelData[pixelMap[yw1 + x * 3 + 2]] = b;

            channelData[pixelMap[yw2 + x * 3]] = r2;
            channelData[pixelMap[yw2 + x * 3 + 1]] = g2;
            channelData[pixelMap[yw2 + x * 3 + 2]] = b2;
        }
    }
}

void LEDPanel::drawNumber(int v, int x, int y, unsigned char* channelData) {
    if (v >= 20) {
        drawNumber(2, x, y, channelData);
        x += 4;
        v -= 20;
    }
    if (v >= 10) {
        drawNumber(1, x, y, channelData);
        x += 2;
        v -= 10;
    }

    int yw1;
    int actualWidth = width;

    if ((orientation == 'L') || (orientation == 'R')) {
        actualWidth = height;
    }

    if (v == 1) {
        for (int yo = 0; yo < 5; yo++) {
            int yw1 = (y + yo) * actualWidth * 3;
            channelData[pixelMap[yw1 + x * 3]] = 255;
            channelData[pixelMap[yw1 + x * 3 + 1]] = 255;
            channelData[pixelMap[yw1 + x * 3 + 2]] = 255;
        }
        return;
    }
    if (v == 4) {
        for (int yo = 0; yo < 5; yo++) {
            int yw1 = (y + yo) * actualWidth * 3;
            channelData[pixelMap[yw1 + (x + 2) * 3]] = 255;
            channelData[pixelMap[yw1 + (x + 2) * 3 + 1]] = 255;
            channelData[pixelMap[yw1 + (x + 2) * 3 + 2]] = 255;
            if (yo < 3) {
                channelData[pixelMap[yw1 + x * 3]] = 255;
                channelData[pixelMap[yw1 + x * 3 + 1]] = 255;
                channelData[pixelMap[yw1 + x * 3 + 2]] = 255;
            }
        }
        yw1 = (y + 2) * actualWidth * 3;
        for (int x2 = 0; x2 < 3; x2++) {
            channelData[pixelMap[yw1 + (x + x2) * 3]] = 255;
            channelData[pixelMap[yw1 + (x + x2) * 3 + 1]] = 255;
            channelData[pixelMap[yw1 + (x + x2) * 3 + 2]] = 255;
        }
        return;
    }
    if (v == 7) {
        for (int yo = 0; yo < 5; yo++) {
            int yw1 = (y + yo) * actualWidth * 3;
            channelData[pixelMap[yw1 + (x + 2) * 3]] = 255;
            channelData[pixelMap[yw1 + (x + 2) * 3 + 1]] = 255;
            channelData[pixelMap[yw1 + (x + 2) * 3 + 2]] = 255;
        }
        yw1 = y * actualWidth * 3;
        for (int x2 = 0; x2 < 3; x2++) {
            channelData[pixelMap[yw1 + (x + x2) * 3]] = 255;
            channelData[pixelMap[yw1 + (x + x2) * 3 + 1]] = 255;
            channelData[pixelMap[yw1 + (x + x2) * 3 + 2]] = 255;
        }
        return;
    }
    yw1 = y * actualWidth * 3;
    for (int x2 = 0; x2 < 3; x2++) {
        channelData[pixelMap[yw1 + (x + x2) * 3]] = 255;
        channelData[pixelMap[yw1 + (x + x2) * 3 + 1]] = 255;
        channelData[pixelMap[yw1 + (x + x2) * 3 + 2]] = 255;

        if (x2 != 1 || v != 0) {
            channelData[pixelMap[yw1 + (x + x2) * 3 + actualWidth * 6]] = 255;
            channelData[pixelMap[yw1 + (x + x2) * 3 + 1 + actualWidth * 6]] = 255;
            channelData[pixelMap[yw1 + (x + x2) * 3 + 2 + actualWidth * 6]] = 255;
        }

        channelData[pixelMap[yw1 + (x + x2) * 3 + actualWidth * 12]] = 255;
        channelData[pixelMap[yw1 + (x + x2) * 3 + 1 + actualWidth * 12]] = 255;
        channelData[pixelMap[yw1 + (x + x2) * 3 + 2 + actualWidth * 12]] = 255;
    }
    yw1 = (y + 1) * actualWidth * 3;
    if (v == 5 || v == 6 || v == 9 || v == 8 || v == 0) {
        channelData[pixelMap[yw1 + x * 3]] = 255;
        channelData[pixelMap[yw1 + x * 3 + 1]] = 255;
        channelData[pixelMap[yw1 + x * 3 + 2]] = 255;
    }
    if (v == 2 || v == 3 || v == 9 || v == 8 || v == 0) {
        channelData[pixelMap[yw1 + (x + 2) * 3]] = 255;
        channelData[pixelMap[yw1 + (x + 2) * 3 + 1]] = 255;
        channelData[pixelMap[yw1 + (x + 2) * 3 + 2]] = 255;
    }
    yw1 = (y + 3) * actualWidth * 3;
    if (v == 2 || v == 6 || v == 8 || v == 0) {
        channelData[pixelMap[yw1 + x * 3]] = 255;
        channelData[pixelMap[yw1 + x * 3 + 1]] = 255;
        channelData[pixelMap[yw1 + x * 3 + 2]] = 255;
    }
    if (v == 3 || v == 5 || v == 6 || v == 8 | v == 9 || v == 0) {
        channelData[pixelMap[yw1 + (x + 2) * 3]] = 255;
        channelData[pixelMap[yw1 + (x + 2) * 3 + 1]] = 255;
        channelData[pixelMap[yw1 + (x + 2) * 3 + 2]] = 255;
    }
}
