/*
 *   BeagleBone Black PRU Matrix handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2018 the Falcon Player Developers
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

#ifndef _BBBMATRIX_H
#define _BBBMATRIX_H

#include <string>

#include "Matrix.h"
#include "PanelMatrix.h"
#include "util/BBBPruUtils.h"

#include "ChannelOutputBase.h"

//16 rows (1/16 scan) * 8bits per row
#define MAX_STATS 16 * 8


class InterleaveHandler;

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;
    
    // write 1 to start, 0xFF to abort. will be cleared when started
    volatile unsigned command;
    volatile unsigned response;
    
    volatile uint16_t pwmBrightness[8];
    uint32_t stats[3 * MAX_STATS]; //3 values per collection
} __attribute__((__packed__)) BBBPruMatrixData;


class BBBMatrix : public ChannelOutputBase {
  public:
    BBBMatrix(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBBMatrix();
    
    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;
    
    virtual void PrepData(unsigned char *channelData) override;
    
    virtual int SendData(unsigned char *channelData) override;
    
    virtual void DumpConfig(void) override;
    
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
    void calcBrightnessFlags(std::vector<std::string> &sargs);
    void printStats();
    bool configureControlPin(const std::string &ctype, Json::Value &root, std::ofstream &outputFile);
    void configurePanelPins(int x, Json::Value &root, std::ofstream &outputFile, int *minPort);
    void configurePanelPin(int x, const std::string &color, int row, Json::Value &root, std::ofstream &outputFile, int *minPort);
    

    BBBPru      *m_pru;
    BBBPru      *m_pruCopy;
    BBBPruMatrixData *m_pruData;
    Matrix      *m_matrix;
    PanelMatrix *m_panelMatrix;
    
    int          m_panelWidth;
    int          m_panelHeight;
    int          m_outputs;
    int          m_longestChain;
    int          m_invertedData;
    int          m_brightness;
    int          m_colorDepth;
    int          m_interleave;
    bool         m_printStats;
    int          m_panelScan; 
    FPPColorOrder m_colorOrder;

    uint32_t     *m_gpioFrame;
    int          m_panels;
    int          m_rows;
    int          m_width;
    int          m_height;
    int          m_rowSize;
    
    bool         m_evenFrame;
    bool         m_outputByRow;
    bool         m_outputBlankData;
    std::vector<int> m_bitOrder;
    
    int          m_timing;
    InterleaveHandler *m_handler;
    
    uint32_t     brightnessValues[12];
    uint32_t     delayValues[12];
    uint16_t     gammaCurve[256];
    
    class GPIOPinInfo {
    public:
        class {
        public:
            uint8_t r_gpio = 0;
            uint32_t r_pin = 0;
            uint8_t g_gpio = 0;
            uint32_t g_pin = 0;
            uint8_t b_gpio = 0;
            uint32_t b_pin = 0;
        } row[2];
    } m_pinInfo[8];
    
    std::vector<std::string> m_usedPins;
    
};

#endif
